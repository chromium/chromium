// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Tests for the AppBarViewController state.
class AppBarViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[AppBarViewController alloc] init];
    [view_controller_ view];
  }

  void TearDown() override {
    view_controller_ = nil;
    PlatformTest::TearDown();
  }

  AppBarViewController* view_controller_;

  // Helper to access the private `_openNewTabButton` ivar using KVC.
  UIButton* openNewTabButton() {
    return [view_controller_ valueForKey:@"openNewTabButton"];
  }
};

// Tests that the new tab button shows the menu as primary action when the
// tab groups page is visible in the tab grid.
TEST_F(AppBarViewControllerTest, TestShowsMenuAsPrimaryActionForTabGroupsPage) {
  [view_controller_ setTabGroupsPageVisible:YES];
  EXPECT_TRUE(openNewTabButton().showsMenuAsPrimaryAction);

  [view_controller_ setTabGroupsPageVisible:NO];
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
}

// Tests that the new tab button shows the menu as primary action when a
// tab group is visible in the tab grid.
TEST_F(AppBarViewControllerTest,
       TestShowsMenuAsPrimaryActionForTabGroupVisible) {
  [view_controller_ setTabGridVisible:YES];
  [view_controller_ setTabGroupVisible:YES];
  EXPECT_TRUE(openNewTabButton().showsMenuAsPrimaryAction);

  [view_controller_ setTabGroupVisible:NO];
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
}

// Tests that the new tab button does NOT show the menu as primary action when a
// tab group is visible but we are not in the tab grid (e.g. browsing).
TEST_F(AppBarViewControllerTest,
       TestShowsMenuAsPrimaryActionForTabGroupVisibleButGridHidden) {
  [view_controller_ setTabGridVisible:NO];
  [view_controller_ setTabGroupVisible:YES];
  EXPECT_FALSE(openNewTabButton().showsMenuAsPrimaryAction);
}

// Tests that rotation toggles stack view distribution, width constraints, and
// spacers.
TEST_F(AppBarViewControllerTest,
       TestRotationTogglesDistributionConstraintsAndSpacers) {
  [view_controller_ updateForAngle:0];

  UIStackView* stackView = [view_controller_ valueForKey:@"stackView"];
  NSArray<NSLayoutConstraint*>* buttonWidthConstraints =
      [view_controller_ valueForKey:@"buttonWidthConstraints"];
  UIView* spacer1 = [view_controller_ valueForKey:@"_leadingSpacer"];
  UIView* spacer2 = [view_controller_ valueForKey:@"_trailingSpacer"];

  EXPECT_EQ(stackView.distribution, UIStackViewDistributionFillEqually);
  for (NSLayoutConstraint* constraint in buttonWidthConstraints) {
    EXPECT_FALSE(constraint.active);
  }
  EXPECT_TRUE(spacer1.hidden);
  EXPECT_TRUE(spacer2.hidden);

  [view_controller_ updateForAngle:M_PI_2];

  EXPECT_EQ(stackView.distribution, UIStackViewDistributionEqualSpacing);
  for (NSLayoutConstraint* constraint in buttonWidthConstraints) {
    EXPECT_TRUE(constraint.active);
  }
  EXPECT_FALSE(spacer1.hidden);
  EXPECT_FALSE(spacer2.hidden);
}

}  // namespace
