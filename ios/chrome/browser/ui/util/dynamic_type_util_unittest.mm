// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/util/dynamic_type_util.h"

#import <UIKit/UIKit.h>

#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for DynamicTypeUtil class.
class DynamicTypeUtilTest : public PlatformTest {
 public:
  DynamicTypeUtilTest() {}
  ~DynamicTypeUtilTest() override {}

  void SetPreferredContentSizeCategory(UILabel* testLabel,
                                       UIViewController* viewController,
                                       UIContentSizeCategory category) {
    UITraitCollection* overrideTraitCollection = [UITraitCollection
        traitCollectionWithPreferredContentSizeCategory:category];
    [viewController.parentViewController
        setOverrideTraitCollection:overrideTraitCollection
            forChildViewController:viewController];
    [testLabel removeFromSuperview];
    [viewController.view addSubview:testLabel];
  }
};

// Checks that the font sizes associated with the "body"
// preferredContentSizeCategory aren't changing in new iOS releases.
TEST_F(DynamicTypeUtilTest, TestFontSize) {
  UIViewController* parentViewController = [[UIViewController alloc] init];
  UIViewController* viewController = [[UIViewController alloc] init];
  [parentViewController addChildViewController:viewController];
  [parentViewController.view addSubview:viewController.view];
  [viewController didMoveToParentViewController:parentViewController];

  UILabel* testLabel = [[UILabel alloc] init];
  testLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  testLabel.adjustsFontForContentSizeCategory = YES;

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryExtraSmall);
  EXPECT_EQ(14.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategorySmall);
  EXPECT_EQ(15.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryMedium);
  EXPECT_EQ(16.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryLarge);
  EXPECT_EQ(17.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryExtraLarge);
  EXPECT_EQ(19.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryExtraExtraLarge);
  EXPECT_EQ(21.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryExtraExtraExtraLarge);
  EXPECT_EQ(23.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryAccessibilityMedium);
  EXPECT_EQ(28.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryAccessibilityLarge);
  EXPECT_EQ(33.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(testLabel, viewController,
                                  UIContentSizeCategoryAccessibilityExtraLarge);
  EXPECT_EQ(40.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(
      testLabel, viewController,
      UIContentSizeCategoryAccessibilityExtraExtraLarge);
  EXPECT_EQ(47.f, testLabel.font.pointSize);

  SetPreferredContentSizeCategory(
      testLabel, viewController,
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);
  EXPECT_EQ(53.f, testLabel.font.pointSize);
}
