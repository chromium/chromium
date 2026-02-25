// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using PageContextWrapperConfigTest = PlatformTest;

// Tests that when flags are disabled, the builder defaults are correct.
TEST_F(PageContextWrapperConfigTest, BuilderDefaults_FlagsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {kPageContextExtractorRefactored, kGeminiRichAPCExtraction});

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder().Build();

  EXPECT_FALSE(config.use_refactored_extractor());
  EXPECT_FALSE(config.graft_cross_origin_frame_content());
  EXPECT_FALSE(config.use_rich_extraction());
}

// Tests that when the refactored extractor flag is enabled, it's reflected in
// the default.
TEST_F(PageContextWrapperConfigTest,
       BuilderDefaults_RefactoredExtractorEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kPageContextExtractorRefactored},
                                       {kGeminiRichAPCExtraction});

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder().Build();

  EXPECT_TRUE(config.use_refactored_extractor());
  EXPECT_FALSE(config.graft_cross_origin_frame_content());
  EXPECT_FALSE(config.use_rich_extraction());
}

// Tests that the builder's setter methods correctly override defaults.
TEST_F(PageContextWrapperConfigTest, BuilderSetters) {
  // Regardless of the feature flag state, explicit setters should work.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {kPageContextExtractorRefactored, kGeminiRichAPCExtraction});

  PageContextWrapperConfig config = PageContextWrapperConfigBuilder()
                                        .SetUseRefactoredExtractor(true)
                                        .SetGraftCrossOriginFrameContent(true)
                                        .SetUseRichExtraction(true)
                                        .Build();

  EXPECT_TRUE(config.use_refactored_extractor());
  EXPECT_TRUE(config.graft_cross_origin_frame_content());
  EXPECT_TRUE(config.use_rich_extraction());
}
