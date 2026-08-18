// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_carousel_slide_view.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class GeminiFirstRunCarouselSlideViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    slide_ = [[GeminiFirstRunCarouselSlide alloc]
        initWithAnimationName:@"FRE_Summarize_Slide"
            darkAnimationName:@"FRE_Summarize_Slide_Dark"
             animationNameRTL:@"FRE_Summarize_Slide_RTL"
         darkAnimationNameRTL:@"FRE_Summarize_Slide_Dark_RTL"
                        title:@"Summarize with Gemini"];
  }

  GeminiFirstRunCarouselSlide* slide_;
};

// Tests that GeminiFirstRunCarouselSlide correctly initializes and stores all
// properties.
TEST_F(GeminiFirstRunCarouselSlideViewTest, SlideProperties) {
  EXPECT_NSEQ(@"FRE_Summarize_Slide", slide_.animationName);
  EXPECT_NSEQ(@"FRE_Summarize_Slide_Dark", slide_.darkAnimationName);
  EXPECT_NSEQ(@"FRE_Summarize_Slide_RTL", slide_.animationNameRTL);
  EXPECT_NSEQ(@"FRE_Summarize_Slide_Dark_RTL", slide_.darkAnimationNameRTL);
  EXPECT_NSEQ(@"Summarize with Gemini", slide_.title);
}

// Tests that the slide view initializes successfully with a valid slide model.
TEST_F(GeminiFirstRunCarouselSlideViewTest, Initialization) {
  GeminiFirstRunCarouselSlideView* slideView =
      [[GeminiFirstRunCarouselSlideView alloc] initWithSlide:slide_];
  EXPECT_NE(slideView, nil);
}

// Tests that animation playback API methods execute cleanly.
TEST_F(GeminiFirstRunCarouselSlideViewTest, PlayAndResetAnimation) {
  GeminiFirstRunCarouselSlideView* slideView =
      [[GeminiFirstRunCarouselSlideView alloc] initWithSlide:slide_];
  EXPECT_NE(slideView, nil);

  [slideView playAnimation];
  [slideView stopAnimation];
}
