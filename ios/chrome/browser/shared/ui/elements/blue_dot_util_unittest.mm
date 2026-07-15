// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/blue_dot_util.h"

#import <UIKit/UIKit.h>

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using BlueDotUtilTest = PlatformTest;

TEST_F(BlueDotUtilTest, ConfigureAndAddBlueDotView_InsertedBelowView) {
  UIButton* containerView = [[UIButton alloc] init];
  [containerView setImage:[[UIImage alloc] init] forState:UIControlStateNormal];

  UIView* blueDot = ConfigureAndAddBlueDotView(containerView);

  ASSERT_TRUE(blueDot != nil);
  EXPECT_EQ(blueDot.superview, containerView);
  EXPECT_FALSE(blueDot.translatesAutoresizingMaskIntoConstraints);
  EXPECT_EQ(blueDot.layer.cornerRadius, 3.0);

  // Verify order: it should be inserted below the imageView.
  NSUInteger blueDotIndex = [containerView.subviews indexOfObject:blueDot];
  NSUInteger insertBelowIndex =
      [containerView.subviews indexOfObject:containerView.imageView];
  EXPECT_LT(blueDotIndex, insertBelowIndex);
}

TEST_F(BlueDotUtilTest, ConfigureAndAddBlueDotView_ModernButton) {
  UIButton* containerView = [[UIButton alloc] init];
  containerView.configuration =
      [UIButtonConfiguration plainButtonConfiguration];

  UIView* blueDot = ConfigureAndAddBlueDotView(containerView);

  ASSERT_TRUE(blueDot != nil);
  EXPECT_EQ(blueDot.superview, containerView);
  // For modern configured buttons, it should be the top-most subview.
  EXPECT_EQ([containerView.subviews lastObject], blueDot);
}

TEST_F(BlueDotUtilTest, UpdateBlueDotMaskForView) {
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 50)];

  // Test adding mask
  UpdateBlueDotMaskForView(view, YES);
  ASSERT_TRUE(view.layer.mask != nil);
  EXPECT_TRUE([view.layer.mask isKindOfClass:[CAShapeLayer class]]);
  CAShapeLayer* shapeLayer = static_cast<CAShapeLayer*>(view.layer.mask);
  EXPECT_NSEQ(shapeLayer.fillRule, kCAFillRuleEvenOdd);

  // Test removing mask
  UpdateBlueDotMaskForView(view, NO);
  EXPECT_TRUE(view.layer.mask == nil);
}
