// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_instructions_view.h"

#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using DefaultBrowserInstructionsViewTest = PlatformTest;

namespace {

bool FindByID(UIView* view, NSString* accessibility_id) {
  if (view.accessibilityIdentifier == accessibility_id) {
    return true;
  }
  for (UIView* subview in view.subviews) {
    if (FindByID(subview, accessibility_id)) {
      return true;
    }
  }
  return false;
}

bool HasTitle(UIView* view) {
  return FindByID(view, kConfirmationAlertTitleAccessibilityIdentifier);
}

bool HasSubTitle(UIView* view) {
  return FindByID(view, kConfirmationAlertSubtitleAccessibilityIdentifier);
}

bool HasInstructionSteps(UIView* view) {
  return FindByID(view,
                  kConfirmationAlertUnderTitleViewAccessibilityIdentifier);
}

bool HasPrimaryActionButton(UIView* view) {
  return FindByID(view, kConfirmationAlertPrimaryActionAccessibilityIdentifier);
}

bool HasSecondaryActionButton(UIView* view) {
  return FindByID(view,
                  kConfirmationAlertSecondaryActionAccessibilityIdentifier);
}

}  // namespace

// Test view creation with subtitle.
TEST_F(DefaultBrowserInstructionsViewTest, CreateViewWithSubtitleTest) {
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] init:NO
                                          hasSteps:NO
                                     actionHandler:nil];
  ASSERT_NE(instructionsView, nil);
  EXPECT_TRUE(HasTitle(instructionsView));
  EXPECT_TRUE(HasSubTitle(instructionsView));
  EXPECT_FALSE(HasInstructionSteps(instructionsView));
  EXPECT_TRUE(HasPrimaryActionButton(instructionsView));
  EXPECT_FALSE(HasSecondaryActionButton(instructionsView));
}

// Test view creation with instruction steps.
TEST_F(DefaultBrowserInstructionsViewTest, CreateViewWithStepsTest) {
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] init:NO
                                          hasSteps:YES
                                     actionHandler:nil];
  ASSERT_NE(instructionsView, nil);
  EXPECT_TRUE(HasTitle(instructionsView));
  EXPECT_FALSE(HasSubTitle(instructionsView));
  EXPECT_TRUE(HasInstructionSteps(instructionsView));
  EXPECT_TRUE(HasPrimaryActionButton(instructionsView));
  EXPECT_FALSE(HasSecondaryActionButton(instructionsView));
}

// Test view creation with secondary button.
TEST_F(DefaultBrowserInstructionsViewTest, CreateViewWithSecondaryButtonTest) {
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] init:YES
                                          hasSteps:NO
                                     actionHandler:nil];
  ASSERT_NE(instructionsView, nil);
  EXPECT_TRUE(HasTitle(instructionsView));
  EXPECT_TRUE(HasSubTitle(instructionsView));
  EXPECT_FALSE(HasInstructionSteps(instructionsView));
  EXPECT_TRUE(HasPrimaryActionButton(instructionsView));
  EXPECT_TRUE(HasSecondaryActionButton(instructionsView));
}
