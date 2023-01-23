// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_creation_impl.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class ResourceCreationImplForTesting : public ResourceCreationImpl {
 public:
  ResourceCreationImplForTesting() : ResourceCreationImpl(nullptr) {}

  ResourceCreationImplForTesting(const ResourceCreationImplForTesting&) =
      delete;
  ResourceCreationImplForTesting& operator=(
      const ResourceCreationImplForTesting&) = delete;

  ~ResourceCreationImplForTesting() override = default;

  // ResourceCreation_API:
  PP_Resource CreateBrowserFont(
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description* description) override {
    return 0;
  }
  PP_Resource CreateFileChooser(PP_Instance instance,
                                PP_FileChooserMode_Dev mode,
                                const PP_Var& accept_types) override {
    return 0;
  }
  PP_Resource CreateFileIO(PP_Instance instance) override { return 0; }
  PP_Resource CreateFileRef(
      PP_Instance instance,
      const ppapi::FileRefCreateInfo& create_info) override {
    return 0;
  }
  PP_Resource CreateFileSystem(PP_Instance instance,
                               PP_FileSystemType type) override {
    return 0;
  }
  PP_Resource CreateGraphics2D(PP_Instance pp_instance,
                               const PP_Size* size,
                               PP_Bool is_always_opaque) override {
    return 0;
  }
  PP_Resource CreatePrinting(PP_Instance instance) override { return 0; }
  PP_Resource CreateURLLoader(PP_Instance instance) override { return 0; }
  PP_Resource CreateURLRequestInfo(PP_Instance instance) override { return 0; }
  PP_Resource CreateWebSocket(PP_Instance instance) override { return 0; }
};

}  // namespace

class ResourceCreationImplTest : public testing::Test {
 public:
  ResourceCreationImplTest() = default;

  void SetUp() override {
    resource_creation_impl_.SetCreateVideoDecoderDevImplCallbackForTesting(
        base::BindRepeating(
            &ResourceCreationImplTest::CreateVideoDecoderDevImpl,
            base::Unretained(this)));
  }

  void TearDown() override {}

 protected:
  bool was_video_decoder_impl_created() {
    return was_video_decoder_impl_created_;
  }

  ResourceCreationImpl* resource_creation_impl() {
    return &resource_creation_impl_;
  }

 private:
  PP_Resource CreateVideoDecoderDevImpl(PP_Instance instance,
                                        PP_Resource graphics_context,
                                        PP_VideoDecoder_Profile profile) {
    was_video_decoder_impl_created_ = true;
    return 0;
  }

  ResourceCreationImplForTesting resource_creation_impl_;
  bool was_video_decoder_impl_created_ = false;
};

TEST_F(ResourceCreationImplTest, APIUnsupportedByDefault) {
  resource_creation_impl()->CreateVideoDecoderDev(
      0, 0, static_cast<PP_VideoDecoder_Profile>(-1));

  EXPECT_FALSE(was_video_decoder_impl_created());
}

TEST_F(ResourceCreationImplTest,
       APISupportedWhenFeatureEnabledAndSwitchNotPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSupportPepperVideoDecoderDevAPI);

  resource_creation_impl()->CreateVideoDecoderDev(
      0, 0, static_cast<PP_VideoDecoder_Profile>(-1));

  EXPECT_TRUE(was_video_decoder_impl_created());
}

TEST_F(ResourceCreationImplTest,
       APINotSupportedWhenFeatureDisabledAndSwitchNotPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kSupportPepperVideoDecoderDevAPI);

  resource_creation_impl()->CreateVideoDecoderDev(
      0, 0, static_cast<PP_VideoDecoder_Profile>(-1));

  EXPECT_FALSE(was_video_decoder_impl_created());
}

TEST_F(ResourceCreationImplTest,
       APISupportedWhenFeatureHasDefaultValueAndSwitchIsPresent) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceEnablePepperVideoDecoderDevAPI);

  resource_creation_impl()->CreateVideoDecoderDev(
      0, 0, static_cast<PP_VideoDecoder_Profile>(-1));

  EXPECT_TRUE(was_video_decoder_impl_created());
}

TEST_F(ResourceCreationImplTest,
       APISupportedWhenFeatureIsEnabledAndSwitchIsPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSupportPepperVideoDecoderDevAPI);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceEnablePepperVideoDecoderDevAPI);

  resource_creation_impl()->CreateVideoDecoderDev(
      0, 0, static_cast<PP_VideoDecoder_Profile>(-1));

  EXPECT_TRUE(was_video_decoder_impl_created());
}

// The command-line switch should override the Feature.
TEST_F(ResourceCreationImplTest,
       APISupportedWhenFeatureIsDisabledButSwitchIsPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kSupportPepperVideoDecoderDevAPI);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceEnablePepperVideoDecoderDevAPI);

  resource_creation_impl()->CreateVideoDecoderDev(
      0, 0, static_cast<PP_VideoDecoder_Profile>(-1));

  EXPECT_TRUE(was_video_decoder_impl_created());
}

}  // namespace content
