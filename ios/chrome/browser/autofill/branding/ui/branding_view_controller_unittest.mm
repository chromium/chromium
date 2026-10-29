// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/branding/ui/branding_view_controller.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Category to expose internal properties for testing.
@interface BrandingViewController (Testing)
@property(nonatomic, readonly) UIButton* brandingIcon;
@property(nonatomic, readonly)
    NSLayoutConstraint* widthConstraintWhenHidingBranding;
@end

class BrandingViewControllerTest : public PlatformTest {
 public:
  BrandingViewControllerTest() {
    view_controller_ = [[BrandingViewController alloc] init];
  }

 protected:
  BrandingViewController* view_controller_;
};

// Tests that `BrandingViewController` starts collapsed with an active 0-width
// constraint upon view load, before any keyboard notification.
TEST_F(BrandingViewControllerTest, StartsCollapsedWithZeroWidthConstraint) {
  [view_controller_ loadViewIfNeeded];

  NSLayoutConstraint* width_constraint =
      view_controller_.widthConstraintWhenHidingBranding;
  ASSERT_NE(width_constraint, nil);
  EXPECT_EQ(width_constraint.constant, 0);
  EXPECT_TRUE(width_constraint.active);
  EXPECT_EQ(view_controller_.brandingIcon.superview, nil);
}

// Tests that `UIKeyboardDidShowNotification` updates branding visibility and
// deactivates the 0-width constraint when branding should be visible.
TEST_F(BrandingViewControllerTest,
       KeyboardDidShowUpdatesBrandingVisibilityWhenVisible) {
  view_controller_.visible = YES;
  view_controller_.keyboardAccessoryVisible = YES;
  [view_controller_ loadViewIfNeeded];

  EXPECT_EQ(view_controller_.brandingIcon.superview, nil);
  EXPECT_TRUE(view_controller_.widthConstraintWhenHidingBranding.active);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardDidShowNotification
                    object:nil];

  EXPECT_EQ(view_controller_.brandingIcon.superview, view_controller_.view);
  EXPECT_FALSE(view_controller_.widthConstraintWhenHidingBranding.active);
}

// Tests that hiding branding removes the branding icon and reactivates the
// 0-width constraint on subsequent keyboard appearances when ineligible.
TEST_F(BrandingViewControllerTest,
       KeyboardDidShowAfterHideCollapsesBrandingWhenIneligible) {
  view_controller_.visible = YES;
  view_controller_.keyboardAccessoryVisible = YES;
  [view_controller_ loadViewIfNeeded];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardDidShowNotification
                    object:nil];
  EXPECT_EQ(view_controller_.brandingIcon.superview, view_controller_.view);
  EXPECT_FALSE(view_controller_.widthConstraintWhenHidingBranding.active);

  // Hide the keyboard and change visibility to ineligible before showing again.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardDidHideNotification
                    object:nil];
  view_controller_.visible = NO;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardDidShowNotification
                    object:nil];
  EXPECT_EQ(view_controller_.brandingIcon.superview, nil);
  EXPECT_TRUE(view_controller_.widthConstraintWhenHidingBranding.active);
}

// Tests that sequential `UIKeyboardWillShowNotification` and
// `UIKeyboardDidShowNotification` calls are idempotent and do not duplicate
// constraints.
TEST_F(BrandingViewControllerTest,
       KeyboardWillShowAndDidShowSequenceIsIdempotent) {
  view_controller_.visible = YES;
  view_controller_.keyboardAccessoryVisible = YES;
  [view_controller_ loadViewIfNeeded];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillShowNotification
                    object:nil];
  EXPECT_EQ(view_controller_.brandingIcon.superview, view_controller_.view);
  EXPECT_FALSE(view_controller_.widthConstraintWhenHidingBranding.active);

  NSUInteger constraint_count = view_controller_.view.constraints.count;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardDidShowNotification
                    object:nil];
  EXPECT_EQ(view_controller_.brandingIcon.superview, view_controller_.view);
  EXPECT_FALSE(view_controller_.widthConstraintWhenHidingBranding.active);
  EXPECT_EQ(view_controller_.view.constraints.count, constraint_count);
}
