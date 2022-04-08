// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_java_script_feature.h"

#import "base/gtest_prod_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class LinkToTextJavaScriptFeatureTest : public PlatformTest {};

TEST_F(LinkToTextJavaScriptFeatureTest, ShouldAttemptIframeGeneration) {
  {
    base::test::ScopedFeatureList feature_on(
        shared_highlighting::kSharedHighlightingAmp);

    EXPECT_TRUE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kIncorrectSelector,
        GURL("https://www.google.com/amp/")));

    // Only kIncorrectSelector should trigger iframe generation. If we found a
    // selection in the main frame, then there won't be one in iframes, so it's
    // pointless to retry in other error cases.
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        {}, GURL("https://www.google.com/amp/")));
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kContextExhausted,
        GURL("https://www.google.com/amp/")));
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kEmptySelection,
        GURL("https://www.google.com/amp/")));
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kUnknown,
        GURL("https://www.google.com/amp/")));
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kTimeout,
        GURL("https://www.google.com/amp/")));

    // Iframe generation is limited to certain domains and paths.
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kIncorrectSelector,
        GURL("https://www.google.com")));
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kIncorrectSelector,
        GURL("https://www.example.com/amp/")));
  }
  {
    base::test::ScopedFeatureList feature_off;
    feature_off.InitAndDisableFeature(
        shared_highlighting::kSharedHighlightingAmp);

    // Retest that the true condition above is false when the feature is off.
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kIncorrectSelector,
        GURL("https://www.google.com/amp/")));
  }
}
