// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_favicons_accordion_view.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ComposeboxFaviconsAccordionViewTest = PlatformTest;

// Helper to create a dummy UIImage for testing.
UIImage* CreateTestImage() {
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(10, 10)];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    UIRectFill(CGRectMake(0, 0, 10, 10));
  }];
}

TEST_F(ComposeboxFaviconsAccordionViewTest, TestInitialization) {
  ComposeboxFaviconsAccordionView* view =
      [[ComposeboxFaviconsAccordionView alloc] initWithFrame:CGRectZero];
  EXPECT_NE(view, nil);
  EXPECT_EQ(view.axis, UILayoutConstraintAxisHorizontal);
  EXPECT_EQ(view.alignment, UIStackViewAlignmentCenter);
  EXPECT_EQ(view.spacing, -4.0);
  EXPECT_FALSE(view.translatesAutoresizingMaskIntoConstraints);
}

TEST_F(ComposeboxFaviconsAccordionViewTest, TestUpdateWithEmptyImages) {
  ComposeboxFaviconsAccordionView* view =
      [[ComposeboxFaviconsAccordionView alloc] initWithFrame:CGRectZero];
  [view updateWithImages:@[]];
  EXPECT_EQ(view.arrangedSubviews.count, 0u);
}

TEST_F(ComposeboxFaviconsAccordionViewTest, TestUpdateWithLessThanMaxImages) {
  ComposeboxFaviconsAccordionView* view =
      [[ComposeboxFaviconsAccordionView alloc] initWithFrame:CGRectZero];
  UIImage* image1 = CreateTestImage();
  UIImage* image2 = CreateTestImage();
  [view updateWithImages:@[ image1, image2 ]];

  EXPECT_EQ(view.arrangedSubviews.count, 2u);
  for (UIView* subview in view.arrangedSubviews) {
    EXPECT_TRUE([subview isKindOfClass:[UIImageView class]]);
  }
}

TEST_F(ComposeboxFaviconsAccordionViewTest, TestUpdateWithExactMaxImages) {
  ComposeboxFaviconsAccordionView* view =
      [[ComposeboxFaviconsAccordionView alloc] initWithFrame:CGRectZero];
  UIImage* image1 = CreateTestImage();
  UIImage* image2 = CreateTestImage();
  UIImage* image3 = CreateTestImage();
  [view updateWithImages:@[ image1, image2, image3 ]];

  EXPECT_EQ(view.arrangedSubviews.count, 3u);
  for (UIView* subview in view.arrangedSubviews) {
    EXPECT_TRUE([subview isKindOfClass:[UIImageView class]]);
  }
}

TEST_F(ComposeboxFaviconsAccordionViewTest, TestUpdateWithMoreThanMaxImages) {
  ComposeboxFaviconsAccordionView* view =
      [[ComposeboxFaviconsAccordionView alloc] initWithFrame:CGRectZero];
  NSMutableArray<UIImage*>* images = [NSMutableArray array];
  for (int i = 0; i < 5; i++) {
    [images addObject:CreateTestImage()];
  }
  [view updateWithImages:images];

  // Maximum displayed icons is 3: 2 image views and 1 overflow badge.
  EXPECT_EQ(view.arrangedSubviews.count, 3u);
  EXPECT_TRUE([view.arrangedSubviews[0] isKindOfClass:[UIImageView class]]);
  EXPECT_TRUE([view.arrangedSubviews[1] isKindOfClass:[UIImageView class]]);
  EXPECT_TRUE([view.arrangedSubviews[2] isKindOfClass:[UILabel class]]);

  UILabel* badge = (UILabel*)view.arrangedSubviews[2];
  EXPECT_NSEQ(badge.text, @"+3");
}

TEST_F(ComposeboxFaviconsAccordionViewTest, TestUpdateClearsPreviousSubviews) {
  ComposeboxFaviconsAccordionView* view =
      [[ComposeboxFaviconsAccordionView alloc] initWithFrame:CGRectZero];
  UIImage* image1 = CreateTestImage();
  UIImage* image2 = CreateTestImage();
  [view updateWithImages:@[ image1, image2 ]];
  EXPECT_EQ(view.arrangedSubviews.count, 2u);

  [view updateWithImages:@[ image1 ]];
  EXPECT_EQ(view.arrangedSubviews.count, 1u);
}

TEST_F(ComposeboxFaviconsAccordionViewTest, TestLoadingState) {
  ComposeboxFaviconsAccordionView* view =
      [[ComposeboxFaviconsAccordionView alloc] initWithFrame:CGRectZero];
  UIImage* image1 = CreateTestImage();
  [view updateWithImages:@[ image1 ]];
  EXPECT_EQ(view.arrangedSubviews.count, 1u);

  view.isLoading = YES;
  EXPECT_EQ(view.arrangedSubviews.count, 1u);
  EXPECT_TRUE(
      [view.arrangedSubviews[0] isKindOfClass:[UIActivityIndicatorView class]]);

  // Updating with images while loading should be ignored.
  [view updateWithImages:@[ image1, image1 ]];
  EXPECT_EQ(view.arrangedSubviews.count, 1u);
  EXPECT_TRUE(
      [view.arrangedSubviews[0] isKindOfClass:[UIActivityIndicatorView class]]);

  view.isLoading = NO;
  EXPECT_EQ(view.arrangedSubviews.count, 0u);
}

}  // namespace
