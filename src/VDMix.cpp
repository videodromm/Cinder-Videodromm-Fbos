#include "VDMix.h"

#include "cinder/gl/Texture.h"
#include "cinder/Xml.h"

using namespace ci;
using namespace ci::app;

namespace VideoDromm {
	VDMix::VDMix(VDSettingsRef aVDSettings, VDAnimationRef aVDAnimation)
		: mFbosPath("fbos.xml")
		, mName("")
		, mFlipV(false)
		, mFlipH(false)
		, mWidth(640)
		, mHeight(480)
	{
		// Settings
		mVDSettings = aVDSettings;
		// Animation
		mVDAnimation = aVDAnimation;
		// initialize the fbo list with audio texture
		initFboList();
		if (mName.length() == 0) {
			mName = mFbosPath;
		}
		mPosX = mPosY = 0.0f;
		mZoom = 1.0f;
		// init the fbo whatever happens next
		gl::Fbo::Format format;
		//format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
		mMixFbo = gl::Fbo::create(mWidth, mHeight, format.depthTexture());
		mLeftFbo = gl::Fbo::create(mWidth, mHeight, format.depthTexture());
		mRightFbo = gl::Fbo::create(mWidth, mHeight, format.depthTexture());
		// init with passthru shader
		mMixShaderName = "mixshader";
		// load shadertoy uniform variables declarations
		shaderInclude = loadString(loadAsset("shadertoy.inc"));
		try
		{
			fs::path vertexFile = getAssetPath("") / "passthru.vert";
			if (fs::exists(vertexFile)) {
				mPassthruVextexShaderString = loadString(loadAsset("passthru.vert"));
				CI_LOG_V("passthru.vert loaded");
			}
			else
			{
				CI_LOG_V("passthru.vert does not exist, should quit");
			}
		}
		catch (gl::GlslProgCompileExc &exc)
		{
			mError = string(exc.what());
			CI_LOG_V("unable to load/compile passthru vertex shader:" + string(exc.what()));
		}
		catch (const std::exception &e)
		{
			mError = string(e.what());
			CI_LOG_V("unable to load passthru vertex shader:" + string(e.what()));
		}
		// load passthru fragment shader
		try
		{
			fs::path fragFile = getAssetPath("") / "mixfbo.frag";
			if (fs::exists(fragFile)) {
				mMixFragmentShaderString = loadString(loadAsset("mixfbo.frag"));
			}
			else
			{
				mError = "mixfbo.frag does not exist, should quit";
				CI_LOG_V(mError);
			}
			mMixShader = gl::GlslProg::create(mPassthruVextexShaderString, mMixFragmentShaderString);
			mMixShader->setLabel(mMixShaderName);
			CI_LOG_V("mixfbo.frag loaded and compiled");
		}
		catch (gl::GlslProgCompileExc &exc)
		{
			mError = string(exc.what());
			CI_LOG_V("unable to load/compile mixfbo fragment shader:" + string(exc.what()));
		}
		catch (const std::exception &e)
		{
			mError = string(e.what());
			CI_LOG_V("unable to load mixfbo fragment shader:" + string(e.what()));
		}
	}
	bool VDMix::initFboList() {
		bool isFirstLaunch = false;
		if (mFboList.size() == 0) {
			CI_LOG_V("VDMix::init mFboList");
			isFirstLaunch = true;
			VDFboRef t(new VDFbo(mVDSettings, mVDAnimation));
			// add details child
			XmlTree			detailsXml;
			detailsXml.setTag("details");
			detailsXml.setAttribute("path", "");
			detailsXml.setAttribute("width", "512");
			detailsXml.setAttribute("height", "2");
			detailsXml.setAttribute("topdown", "0");
			detailsXml.setAttribute("uselinein", "1");
			// add texture child
			XmlTree			textureXml;
			textureXml.setTag("texture");
			textureXml.setAttribute("id", "0");
			textureXml.setAttribute("texturetype", "audio");
			textureXml.push_back(detailsXml);
			// create fbo xml
			XmlTree			fboXml;
			fboXml.setTag("fbo");
			fboXml.setAttribute("id", "0");
			fboXml.setAttribute("width", "640");
			fboXml.setAttribute("height", "480");
			fboXml.push_back(textureXml);
			t->fromXml(fboXml);
			mFboList.push_back(t);
		}
		return isFirstLaunch;
	}

	VDMix::~VDMix(void) {

	}
	int VDMix::loadFboFragmentShader(string aFilePath) {
		return mFboList[0]->loadFragmentShader(aFilePath);
	}
	string VDMix::getFboFragmentShaderText(unsigned int aFboIndex) {
		return mFboList[0]->getFragmentShaderText(aFboIndex);
	}
	VDMixList VDMix::readSettings(VDSettingsRef aVDSettings, VDAnimationRef aVDAnimation, const DataSourceRef &source) {
		XmlTree			doc;
		VDMixList	VDMixlist;

		CI_LOG_V("VDMix readSettings");
		// try to load the specified xml file
		try {
			doc = XmlTree(source);
			CI_LOG_V("VDMix xml doc ok");
		}
		catch (...) { return VDMixlist; }

		// check if this is a valid file 
		bool isOK = doc.hasChild("mixes");
		if (!isOK) return VDMixlist;

		//
		if (isOK) {

			XmlTree mixXml = doc.getChild("mixes");

			// iterate textures
			for (XmlTree::ConstIter child = mixXml.begin("mix"); child != mixXml.end(); ++child) {
				// add the mix to the list
				CI_LOG_V("Add Mix " + child->getValue());
				VDMixRef t(new VDMix(aVDSettings, aVDAnimation));
				t->fromXml(*child);
				VDMixlist.push_back(t);
			}
		}

		return VDMixlist;
	}

	void VDMix::writeSettings(const VDMixList &VDMixlist, const ci::DataTargetRef &target) {

		// create config document and root <textures>
		/* TODO XmlTree			doc;
		doc.setTag("mixes");
		doc.setAttribute("version", "1.0");

		//
		for (unsigned int i = 0; i < VDMixlist.size(); ++i) {
		// create <texture>
		XmlTree			mix;
		mix.setTag("fbo");
		mix.setAttribute("id", i + 1);
		// details specific to texture type
		mix.push_back(VDMixlist[i]->toXml());

		// add fbo to doc
		doc.push_back(mix);
		}

		// write file
		doc.write(target);*/
	}
	XmlTree	VDMix::toXml() const
	{
		XmlTree		xml;
		xml.setTag("details");
		// TODO rewrite xml.setAttribute("fbopath", mFbosPath);
		xml.setAttribute("width", mWidth);
		xml.setAttribute("height", mHeight);
		return xml;
	}

	void VDMix::fromXml(const XmlTree &xml)
	{
		// initialize the fbo list with audio texture
		bool isFirstLaunch = initFboList();
		// find fbo childs in xml
		if (xml.hasChild("fbo")) {
			CI_LOG_V("VDMix got fbo child ");
			for (XmlTree::ConstIter fboChild = xml.begin("fbo"); fboChild != xml.end(); ++fboChild) {
				CI_LOG_V("VDMix create fbo ");
				VDFboRef t(new VDFbo(mVDSettings, mVDAnimation));
				t->fromXml(*fboChild);
				/* TODO 
				if (isFirstLaunch) {
					mFboList[0] = t;
				}
				else {*/
					mFboList.push_back(t);
				//}
			}
		}
	}
	void VDMix::setPosition(int x, int y) {
		mPosX = ((float)x / (float)mWidth) - 0.5;
		mPosY = ((float)y / (float)mHeight) - 0.5;
		for (auto &fbo : mFboList)
		{
			fbo->setPosition(mPosX, mPosY);
		}
	}
	void VDMix::setZoom(float aZoom) {
		mZoom = aZoom;
		for (auto &fbo : mFboList)
		{
			fbo->setZoom(mZoom);
		}
	}
	int VDMix::getTextureWidth() {
		return mWidth;
	};

	int VDMix::getTextureHeight() {
		return mHeight;
	};
	unsigned int VDMix::getInputTexturesCount(unsigned int aFboIndex) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		return mFboList[aFboIndex]->getInputTexturesCount();
	}
	string VDMix::getInputTextureName(unsigned int aFboIndex, unsigned int aTextureIndex) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		return mFboList[aFboIndex]->getInputTextureName(aTextureIndex);
	}
	string VDMix::getFboName(unsigned int aFboIndex) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		return mFboList[aFboIndex]->getName();
	}
	string VDMix::getFboLabel(unsigned int aFboIndex) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		return mFboList[aFboIndex]->getLabel();
	}
	int VDMix::getFboTextureWidth(unsigned int aFboIndex) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		return mFboList[aFboIndex]->getTextureWidth();
	};

	int VDMix::getFboTextureHeight(unsigned int aFboIndex) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		return mFboList[aFboIndex]->getTextureHeight();
	};

	ci::ivec2 VDMix::getSize() {
		return mMixFbo->getSize();
	}

	ci::Area VDMix::getBounds() {
		return mMixFbo->getBounds();
	}

	GLuint VDMix::getId() {
		return mMixFbo->getId();
	}

	std::string VDMix::getName(){
		return mName;
	}
	// Render left FBO
	void VDMix::renderLeftFbo()
	{
		gl::ScopedFramebuffer fbScp(mLeftFbo);
		// clear out the FBO with blue
		gl::clear(Color(0.25, 0.5f, 1.0f));

		// setup the viewport to match the dimensions of the FBO
		gl::ScopedViewport scpVp(ivec2(30), mLeftFbo->getSize());

		// render the left fbo
		gl::ScopedGlslProg shaderScp(gl::getStockShader(gl::ShaderDef().texture()));
		gl::ScopedTextureBind tex(mFboList[0]->getTexture());
		gl::drawSolidRect(Rectf(0, 40, mWidth, mHeight));
	}
	// Render left FBO
	void VDMix::renderRightFbo()
	{
		gl::ScopedFramebuffer fbScp(mRightFbo);
		// clear out the FBO with red
		gl::clear(Color(0.5, 0.0f, 0.0f));

		// setup the viewport to match the dimensions of the FBO
		gl::ScopedViewport scpVp(ivec2(30), mRightFbo->getSize());

		// render the right fbo
		gl::ScopedGlslProg shaderScp(gl::getStockShader(gl::ShaderDef().texture()));
		if (mFboList.size() > 1) {
			gl::ScopedTextureBind tex(mFboList[1]->getTexture());
		}
		else {
			CI_LOG_W("renderRightFbo: only one fbo, right renders left...");
			gl::ScopedTextureBind tex(mFboList[0]->getTexture());
		}
		gl::drawSolidRect(Rectf(40, 0, mWidth, mHeight));
	}
	ci::gl::TextureRef VDMix::getRightFboTexture() {
		return mRightFbo->getColorTexture();
	}
	ci::gl::TextureRef VDMix::getLeftFboTexture() {
		return mLeftFbo->getColorTexture();
	}
	void VDMix::loadImageFile(string aFile, unsigned int aFboIndex, unsigned int aTextureIndex, bool right) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		CI_LOG_V("fbolist" + toString(aFboIndex) + ": loadImageFile " + aFile + " at textureIndex " + toString(aTextureIndex));
		mFboList[aFboIndex]->loadImageFile(aFile, aTextureIndex);
	}

	ci::gl::Texture2dRef VDMix::getFboTexture(unsigned int aFboIndex) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		return mFboList[aFboIndex]->getTexture();
	}
	ci::gl::Texture2dRef VDMix::getFboInputTexture(unsigned int aFboIndex, unsigned int aFboInputTextureIndex) {
		if (aFboIndex > mFboList.size() - 1) aFboIndex = mFboList.size() - 1;
		return mFboList[aFboIndex]->getInputTexture(aFboInputTextureIndex);
	}
	void VDMix::setCrossfade(float aCrossfade) {
		mVDAnimation->controlValues[21] = aCrossfade;
	}
	ci::gl::TextureRef VDMix::getTexture() {
		renderLeftFbo();
		renderRightFbo();
		iChannelResolution0 = vec3(mPosX, mPosY, 0.5);
		gl::ScopedFramebuffer fbScp(mMixFbo);
		gl::clear(Color::white());
		// setup the viewport to match the dimensions of the FBO
		gl::ScopedViewport scpVp(ivec2(5), mMixFbo->getSize());
		gl::ScopedGlslProg shaderScp(mMixShader);
		mRightFbo->bindTexture(0);
		mLeftFbo->bindTexture(1);

		mMixShader->uniform("iGlobalTime", mVDSettings->iGlobalTime);
		mMixShader->uniform("iResolution", vec3(mWidth, mHeight, 1.0));
		mMixShader->uniform("iChannelResolution[0]", iChannelResolution0);
		mMixShader->uniform("iMouse", vec4(mVDSettings->mRenderPosXY.x, mVDSettings->mRenderPosXY.y, mVDSettings->iMouse.z, mVDSettings->iMouse.z));//iMouse =  Vec3i( event.getX(), mRenderHeight - event.getY(), 1 );
		mMixShader->uniform("iZoom", mZoom);
		mMixShader->uniform("iChannel0", 0);
		mMixShader->uniform("iChannel1", 1);
		mMixShader->uniform("iAudio0", 0);
		mMixShader->uniform("iFreq0", mVDAnimation->iFreqs[0]);
		mMixShader->uniform("iFreq1", mVDAnimation->iFreqs[1]);
		mMixShader->uniform("iFreq2", mVDAnimation->iFreqs[2]);
		mMixShader->uniform("iFreq3", mVDAnimation->iFreqs[3]);
		mMixShader->uniform("iChannelTime", mVDSettings->iChannelTime, 4);
		mMixShader->uniform("iColor", vec3(mVDAnimation->controlValues[1], mVDAnimation->controlValues[2], mVDAnimation->controlValues[3]));// mVDSettings->iColor);
		mMixShader->uniform("iBackgroundColor", vec3(mVDAnimation->controlValues[5], mVDAnimation->controlValues[6], mVDAnimation->controlValues[7]));// mVDSettings->iBackgroundColor);
		mMixShader->uniform("iSteps", (int)mVDAnimation->controlValues[20]);
		mMixShader->uniform("iRatio", mVDAnimation->controlValues[11]);//check if needed: +1;//mVDSettings->iRatio); 
		mMixShader->uniform("width", 1);
		mMixShader->uniform("height", 1);
		mMixShader->uniform("iRenderXY", vec2(0.0, 0.0));
		mMixShader->uniform("iAlpha", mVDAnimation->controlValues[4]);
		mMixShader->uniform("iBlendmode", mVDSettings->iBlendMode);
		mMixShader->uniform("iRotationSpeed", mVDAnimation->controlValues[19]);
		mMixShader->uniform("iCrossfade", mVDAnimation->controlValues[21]);
		mMixShader->uniform("iPixelate", mVDAnimation->controlValues[15]);
		mMixShader->uniform("iExposure", mVDAnimation->controlValues[14]);
		mMixShader->uniform("iFade", (int)mVDSettings->iFade);
		mMixShader->uniform("iToggle", (int)mVDAnimation->controlValues[46]);
		mMixShader->uniform("iLight", (int)mVDSettings->iLight);
		mMixShader->uniform("iLightAuto", (int)mVDSettings->iLightAuto);
		mMixShader->uniform("iGreyScale", (int)mVDSettings->iGreyScale);
		mMixShader->uniform("iTransition", mVDSettings->iTransition);
		mMixShader->uniform("iAnim", mVDSettings->iAnim.value());
		mMixShader->uniform("iRepeat", (int)mVDSettings->iRepeat);
		mMixShader->uniform("iVignette", (int)mVDAnimation->controlValues[47]);
		mMixShader->uniform("iInvert", (int)mVDAnimation->controlValues[48]);
		mMixShader->uniform("iDebug", (int)mVDSettings->iDebug);
		mMixShader->uniform("iShowFps", (int)mVDSettings->iShowFps);
		mMixShader->uniform("iFps", mVDSettings->iFps);
		// TODO mMixShader->uniform("iDeltaTime", mVDAnimation->iDeltaTime);
		// TODO mMixShader->uniform("iTempoTime", mVDAnimation->iTempoTime);
		mMixShader->uniform("iTempoTime", 1.0f);
		mMixShader->uniform("iGlitch", (int)mVDAnimation->controlValues[45]);
		mMixShader->uniform("iBeat", mVDSettings->iBeat);
		mMixShader->uniform("iSeed", mVDSettings->iSeed);
		mMixShader->uniform("iFlipH", mFlipH);
		mMixShader->uniform("iFlipV", mFlipV);


		mMixShader->uniform("iTrixels", mVDAnimation->controlValues[16]);
		mMixShader->uniform("iGridSize", mVDAnimation->controlValues[17]);
		mMixShader->uniform("iRedMultiplier", mVDSettings->iRedMultiplier);
		mMixShader->uniform("iGreenMultiplier", mVDSettings->iGreenMultiplier);
		mMixShader->uniform("iBlueMultiplier", mVDSettings->iBlueMultiplier);
		mMixShader->uniform("iParam1", mVDSettings->iParam1);
		mMixShader->uniform("iParam2", mVDSettings->iParam2);
		mMixShader->uniform("iXorY", mVDSettings->iXorY);
		mMixShader->uniform("iBadTv", mVDSettings->iBadTv);

		gl::drawSolidRect(Rectf(0, 0, mWidth, mHeight));
		return mMixFbo->getColorTexture();
	}
} // namespace VideoDromm
