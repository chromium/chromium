// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_carousel_slide_view.h"

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class GeminiFirstRunCarouselSlideViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    dynamic_slide_ = [[GeminiFirstRunCarouselSlide alloc]
              initWithAnimationName:kLottieAnimationFRESummarizeSlideName
                   animationNameRTL:kLottieAnimationFRESummarizeSlideRTLName
                              title:@"Summarize with Gemini"
        animationAccessibilityLabel:@"Summarize artwork"
             textProviderDictionary:@{@"key" : @"value"}
             lightModeColorProvider:@{@"color_key" : UIColor.whiteColor}
              darkModeColorProvider:@{@"color_key" : UIColor.blackColor}];

    static_slide_ = [[GeminiFirstRunCarouselSlide alloc]
              initWithAnimationName:kLottieAnimationFREShoppingSlideName
                  darkAnimationName:kLottieAnimationFREShoppingSlideDarkName
                   animationNameRTL:kLottieAnimationFREShoppingSlideRTLName
               darkAnimationNameRTL:kLottieAnimationFREShoppingSlideDarkRTLName
                              title:@"Shop with Gemini"
        animationAccessibilityLabel:@"Shop artwork"
             textProviderDictionary:nil];
  }

  GeminiFirstRunCarouselSlide* dynamic_slide_;
  GeminiFirstRunCarouselSlide* static_slide_;
};

// Tests that GeminiFirstRunCarouselSlide correctly initializes and stores
// dynamic color properties.
TEST_F(GeminiFirstRunCarouselSlideViewTest, DynamicSlideProperties) {
  EXPECT_NSEQ(kLottieAnimationFRESummarizeSlideName,
              dynamic_slide_.animationName);
  EXPECT_NSEQ(kLottieAnimationFRESummarizeSlideRTLName,
              dynamic_slide_.animationNameRTL);
  EXPECT_NSEQ(nil, dynamic_slide_.darkAnimationName);
  EXPECT_NSEQ(nil, dynamic_slide_.darkAnimationNameRTL);
  EXPECT_NSEQ(@"Summarize with Gemini", dynamic_slide_.title);
  EXPECT_NSEQ(@"Summarize artwork", dynamic_slide_.animationAccessibilityLabel);
  EXPECT_NSEQ(@"value", dynamic_slide_.textProviderDictionary[@"key"]);
}

// Tests that GeminiFirstRunCarouselSlide correctly initializes and stores
// static 4-asset properties.
TEST_F(GeminiFirstRunCarouselSlideViewTest, StaticSlideProperties) {
  EXPECT_NSEQ(kLottieAnimationFREShoppingSlideName,
              static_slide_.animationName);
  EXPECT_NSEQ(kLottieAnimationFREShoppingSlideDarkName,
              static_slide_.darkAnimationName);
  EXPECT_NSEQ(kLottieAnimationFREShoppingSlideRTLName,
              static_slide_.animationNameRTL);
  EXPECT_NSEQ(kLottieAnimationFREShoppingSlideDarkRTLName,
              static_slide_.darkAnimationNameRTL);
  EXPECT_NSEQ(@"Shop with Gemini", static_slide_.title);
  EXPECT_NSEQ(@"Shop artwork", static_slide_.animationAccessibilityLabel);
}

// Tests that the slide view initializes successfully with a valid slide model.
TEST_F(GeminiFirstRunCarouselSlideViewTest, Initialization) {
  GeminiFirstRunCarouselSlideView* dynamicSlideView =
      [[GeminiFirstRunCarouselSlideView alloc] initWithSlide:dynamic_slide_];
  EXPECT_NE(dynamicSlideView, nil);

  GeminiFirstRunCarouselSlideView* staticSlideView =
      [[GeminiFirstRunCarouselSlideView alloc] initWithSlide:static_slide_];
  EXPECT_NE(staticSlideView, nil);
}

// Tests that animation playback API methods execute cleanly.
TEST_F(GeminiFirstRunCarouselSlideViewTest, PlayAndResetAnimation) {
  GeminiFirstRunCarouselSlideView* slideView =
      [[GeminiFirstRunCarouselSlideView alloc] initWithSlide:dynamic_slide_];
  EXPECT_NE(slideView, nil);

  [slideView playAnimation];
  [slideView stopAnimation];
}
