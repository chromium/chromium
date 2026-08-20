// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_carousel_view.h"

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

UIScrollView* GetScrollView(GeminiFirstRunCarouselView* carousel) {
  for (UIView* subview in carousel.subviews) {
    if (UIScrollView* scroll_view =
            base::apple::ObjCCast<UIScrollView>(subview)) {
      return scroll_view;
    }
  }
  return nil;
}

UIPageControl* GetPageControl(GeminiFirstRunCarouselView* carousel) {
  for (UIView* subview in carousel.subviews) {
    if (UIPageControl* page_control =
            base::apple::ObjCCast<UIPageControl>(subview)) {
      return page_control;
    }
  }
  return nil;
}

}  // namespace

@interface GeminiFirstRunCarouselView (Testing)
- (CGFloat)contentOffsetXForPage:(NSInteger)page;
- (NSInteger)pageIndexForContentOffset:(CGFloat)offsetX;
- (BOOL)isRTL;
@end

class GeminiFirstRunCarouselViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    slide1_ = [[GeminiFirstRunCarouselSlide alloc]
              initWithAnimationName:kLottieAnimationFRESummarizeSlideName
                  darkAnimationName:kLottieAnimationFRESummarizeSlideDarkName
                   animationNameRTL:kLottieAnimationFRESummarizeSlideRTLName
               darkAnimationNameRTL:kLottieAnimationFRESummarizeSlideDarkRTLName
                              title:@"Summarize with Gemini"
        animationAccessibilityLabel:@"Summarize artwork"];
    slide2_ = [[GeminiFirstRunCarouselSlide alloc]
              initWithAnimationName:kLottieAnimationFREShoppingSlideName
                  darkAnimationName:kLottieAnimationFREShoppingSlideDarkName
                   animationNameRTL:kLottieAnimationFREShoppingSlideRTLName
               darkAnimationNameRTL:kLottieAnimationFREShoppingSlideDarkRTLName
                              title:@"Shop with Gemini"
        animationAccessibilityLabel:@"Shop artwork"];
    slide3_ = [[GeminiFirstRunCarouselSlide alloc]
              initWithAnimationName:kLottieAnimationFREPlanningSlideName
                  darkAnimationName:kLottieAnimationFREPlanningSlideDarkName
                   animationNameRTL:kLottieAnimationFREPlanningSlideRTLName
               darkAnimationNameRTL:kLottieAnimationFREPlanningSlideDarkRTLName
                              title:@"Plan with Gemini"
        animationAccessibilityLabel:@"Plan artwork"];
  }

  base::test::TaskEnvironment task_environment_;
  GeminiFirstRunCarouselSlide* slide1_;
  GeminiFirstRunCarouselSlide* slide2_;
  GeminiFirstRunCarouselSlide* slide3_;
};

// Tests that the carousel initializes correctly with slides and sets initial
// page state.
TEST_F(GeminiFirstRunCarouselViewTest, InitializationAndSlideCount) {
  GeminiFirstRunCarouselView* carousel = [[GeminiFirstRunCarouselView alloc]
      initWithSlides:@[ slide1_, slide2_, slide3_ ]];

  EXPECT_NE(carousel, nil);
  UIPageControl* pageControl = GetPageControl(carousel);
  ASSERT_NE(pageControl, nil);
  EXPECT_EQ(3, pageControl.numberOfPages);
  EXPECT_EQ(0, pageControl.currentPage);
}

// Tests that public API lifecycle methods execute cleanly without errors.
TEST_F(GeminiFirstRunCarouselViewTest, PublicAPIContracts) {
  GeminiFirstRunCarouselView* carousel = [[GeminiFirstRunCarouselView alloc]
      initWithSlides:@[ slide1_, slide2_, slide3_ ]];

  [carousel startAutoScrolling];
  [carousel stopAutoScrolling];
  [carousel prepareForSizeTransition];
  [carousel completeSizeTransition];
  [carousel recenterActiveSlide];
}

// Tests content offset calculations for LTR and RTL.
TEST_F(GeminiFirstRunCarouselViewTest, ContentOffsetXCalculations) {
  GeminiFirstRunCarouselView* carousel = [[GeminiFirstRunCarouselView alloc]
      initWithSlides:@[ slide1_, slide2_, slide3_ ]];
  carousel.frame = CGRectMake(0, 0, 375, 266);
  [carousel layoutIfNeeded];

  CGFloat pageWidth = GetScrollView(carousel).bounds.size.width;
  ASSERT_GT(pageWidth, 0);

  // In LTR:
  // Slide 0 -> 0 * pageWidth
  // Slide 1 -> 1 * pageWidth
  // Slide 2 -> 2 * pageWidth
  EXPECT_EQ(0 * pageWidth, [carousel contentOffsetXForPage:0]);
  EXPECT_EQ(1 * pageWidth, [carousel contentOffsetXForPage:1]);
  EXPECT_EQ(2 * pageWidth, [carousel contentOffsetXForPage:2]);

  // In RTL:
  carousel.semanticContentAttribute =
      UISemanticContentAttributeForceRightToLeft;
  // Slide 0 -> 2 * pageWidth
  // Slide 1 -> 1 * pageWidth
  // Slide 2 -> 0 * pageWidth
  EXPECT_EQ(2 * pageWidth, [carousel contentOffsetXForPage:0]);
  EXPECT_EQ(1 * pageWidth, [carousel contentOffsetXForPage:1]);
  EXPECT_EQ(0 * pageWidth, [carousel contentOffsetXForPage:2]);
}

// Tests page index mapping from content offsets in LTR and RTL.
TEST_F(GeminiFirstRunCarouselViewTest, PageIndexForContentOffset) {
  GeminiFirstRunCarouselView* carousel = [[GeminiFirstRunCarouselView alloc]
      initWithSlides:@[ slide1_, slide2_, slide3_ ]];
  carousel.frame = CGRectMake(0, 0, 375, 266);
  [carousel layoutIfNeeded];

  CGFloat pageWidth = GetScrollView(carousel).bounds.size.width;
  ASSERT_GT(pageWidth, 0);

  // In LTR:
  EXPECT_EQ(0, [carousel pageIndexForContentOffset:0 * pageWidth]);
  EXPECT_EQ(1, [carousel pageIndexForContentOffset:1 * pageWidth]);
  EXPECT_EQ(2, [carousel pageIndexForContentOffset:2 * pageWidth]);

  // In RTL:
  carousel.semanticContentAttribute =
      UISemanticContentAttributeForceRightToLeft;
  EXPECT_EQ(2, [carousel pageIndexForContentOffset:0 * pageWidth]);
  EXPECT_EQ(1, [carousel pageIndexForContentOffset:1 * pageWidth]);
  EXPECT_EQ(0, [carousel pageIndexForContentOffset:2 * pageWidth]);
}
