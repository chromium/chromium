// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/metrics_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using PageContextWrapperConfigTest = PlatformTest;

// Tests that when flags are disabled, the builder defaults are correct.
TEST_F(PageContextWrapperConfigTest, BuilderDefaults_FlagsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {kPageContextExtractorRefactored,
           kPageContextScreenshotPasswordRedaction});

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder().Build();

  EXPECT_FALSE(config.use_refactored_extractor());
  EXPECT_FALSE(config.graft_cross_origin_frame_content());
  EXPECT_FALSE(config.use_rich_extraction());
  EXPECT_FALSE(config.use_rich_extraction_with_actionable());
  EXPECT_FALSE(config.extract_password_screenshot_redactions());
  EXPECT_TRUE(config.block_unsafe_pages());
}

// Tests that when the refactored extractor flag is enabled, it's reflected in
// the default.
TEST_F(PageContextWrapperConfigTest,
       BuilderDefaults_RefactoredExtractorEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageContextExtractorRefactored}, {});

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder().Build();

  EXPECT_TRUE(config.use_refactored_extractor());
  EXPECT_FALSE(config.graft_cross_origin_frame_content());
  EXPECT_FALSE(config.use_rich_extraction());
  EXPECT_FALSE(config.use_rich_extraction_with_actionable());
}

// Tests that the builder's setter methods correctly override defaults.
TEST_F(PageContextWrapperConfigTest, BuilderSetters) {
  // Regardless of the feature flag state, explicit setters should work.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {kPageContextExtractorRefactored,
           kPageContextScreenshotPasswordRedaction});

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder()
          .SetUseRefactoredExtractor(true)
          .SetGraftCrossOriginFrameContent(true)
          .SetUseRichExtraction(true)
          .SetUseRichExtractionWithActionable(true)
          .SetExtractPasswordScreenshotRedactions(true)
          .Build();

  EXPECT_TRUE(config.use_refactored_extractor());
  EXPECT_TRUE(config.graft_cross_origin_frame_content());
  EXPECT_TRUE(config.use_rich_extraction());
  EXPECT_TRUE(config.use_rich_extraction_with_actionable());
  EXPECT_TRUE(config.extract_password_screenshot_redactions());
}

// Tests that extract_password_screenshot_redactions reflects feature flag
// defaults and builder overrides.
TEST_F(PageContextWrapperConfigTest,
       ExtractPasswordScreenshotRedactions_FallbackAndOverride) {
  // 1. When flag is enabled, default is true.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        kPageContextScreenshotPasswordRedaction);

    PageContextWrapperConfig default_config =
        PageContextWrapperConfigBuilder().Build();
    EXPECT_TRUE(default_config.extract_password_screenshot_redactions());
  }

  // 2. When flag is disabled, default is false and explicit true overrides it.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        kPageContextScreenshotPasswordRedaction);

    PageContextWrapperConfig default_config =
        PageContextWrapperConfigBuilder().Build();
    EXPECT_FALSE(default_config.extract_password_screenshot_redactions());

    PageContextWrapperConfig overridden_config =
        PageContextWrapperConfigBuilder()
            .SetExtractPasswordScreenshotRedactions(true)
            .Build();
    EXPECT_TRUE(overridden_config.extract_password_screenshot_redactions());
  }
}

// Tests the different ways to enable cross origin frame content grafting.
TEST_F(PageContextWrapperConfigTest, GraftCrossOriginFrameContent) {
  {
    PageContextWrapperConfig config = PageContextWrapperConfigBuilder()
                                          .SetGraftCrossOriginFrameContent(true)
                                          .Build();
    EXPECT_TRUE(config.graft_cross_origin_frame_content());
  }
  {
    PageContextWrapperConfig config =
        PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();
    EXPECT_TRUE(config.graft_cross_origin_frame_content());
  }
  {
    PageContextWrapperConfig config =
        PageContextWrapperConfigBuilder()
            .SetUseRichExtractionWithActionable(true)
            .Build();
    EXPECT_TRUE(config.graft_cross_origin_frame_content());
  }
}

// Tests that GetApcConfigVariant returns the correct string based on config.
TEST_F(PageContextWrapperConfigTest, GetApcConfigVariant) {
  // Default (InnerTextOnly).
  {
    PageContextWrapperConfig config = PageContextWrapperConfigBuilder().Build();
    EXPECT_EQ(config.GetApcConfigVariant(),
              kPageContextAPCConfigVariantInnerText);
  }

  // Rich.
  {
    PageContextWrapperConfig config =
        PageContextWrapperConfigBuilder().SetUseRichExtraction(true).Build();
    EXPECT_EQ(config.GetApcConfigVariant(), kPageContextAPCConfigVariantRich);
  }

  // RichAndActionable.
  {
    PageContextWrapperConfig config =
        PageContextWrapperConfigBuilder()
            .SetUseRichExtractionWithActionable(true)
            .Build();
    EXPECT_EQ(config.GetApcConfigVariant(),
              kPageContextAPCConfigVariantRichActionable);
  }
}

// Tests that SetDefaultRichExtraction configures all rich extraction defaults.
TEST_F(PageContextWrapperConfigTest, SetDefaultRichExtraction) {
  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetDefaultRichExtraction(true).Build();

  EXPECT_TRUE(config.use_rich_extraction());
  EXPECT_TRUE(config.graft_cross_origin_frame_content());
  EXPECT_TRUE(config.extract_paid_content());
  EXPECT_FALSE(config.use_rich_extraction_with_actionable());
}
