// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view.h"

#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using DefaultBrowserInstructionsViewTest = PlatformTest;

namespace {

UIView* FindByID(UIView* view, NSString* accessibility_id) {
  if (view.accessibilityIdentifier == accessibility_id) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* foundView = FindByID(subview, accessibility_id);
    if (foundView) {
      return foundView;
    }
  }
  return nil;
}

UIView* GetAnimationSubview(UIView* view) {
  return FindByID(view, kDefaultBrowserInstructionsViewAnimationViewId);
}

UIView* GetDarkAnimationSubview(UIView* view) {
  return FindByID(view, kDefaultBrowserInstructionsViewDarkAnimationViewId);
}

bool HasTitle(UIView* view) {
  return FindByID(view, kConfirmationAlertTitleAccessibilityIdentifier) != nil;
}

bool HasSubTitle(UIView* view) {
  return FindByID(view, kConfirmationAlertSubtitleAccessibilityIdentifier) !=
         nil;
}

bool HasInstructionSteps(UIView* view) {
  return FindByID(view,
                  kConfirmationAlertUnderTitleViewAccessibilityIdentifier) !=
         nil;
}

bool HasPrimaryActionButton(UIView* view) {
  return FindByID(view,
                  kConfirmationAlertPrimaryActionAccessibilityIdentifier) !=
         nil;
}

bool HasSecondaryActionButton(UIView* view) {
  return FindByID(view,
                  kConfirmationAlertSecondaryActionAccessibilityIdentifier) !=
         nil;
}

bool HasTertiaryActionButton(UIView* view) {
  return FindByID(view,
                  kConfirmationAlertTertiaryActionAccessibilityIdentifier) !=
         nil;
}
}  // namespace

// Test view creation with subtitle.
TEST_F(DefaultBrowserInstructionsViewTest, CreateViewWithSubtitleTest) {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] initWithDismissButton:NO
                                                   hasRemindMeLater:NO
                                                           hasSteps:NO
                                                      actionHandler:nil
                                          alertScreenViewController:alertScreen
                                                          titleText:nil];
  ASSERT_NE(instructionsView, nil);
  EXPECT_TRUE(HasTitle(instructionsView));
  EXPECT_TRUE(HasSubTitle(instructionsView));
  EXPECT_FALSE(HasInstructionSteps(instructionsView));
  EXPECT_TRUE(HasPrimaryActionButton(instructionsView));
  EXPECT_FALSE(HasSecondaryActionButton(instructionsView));
  EXPECT_FALSE(HasTertiaryActionButton(instructionsView));
}

// Test view creation with instruction steps.
TEST_F(DefaultBrowserInstructionsViewTest, CreateViewWithStepsTest) {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] initWithDismissButton:NO
                                                   hasRemindMeLater:NO
                                                           hasSteps:YES
                                                      actionHandler:nil
                                          alertScreenViewController:alertScreen
                                                          titleText:nil];
  ASSERT_NE(instructionsView, nil);
  EXPECT_TRUE(HasTitle(instructionsView));
  EXPECT_FALSE(HasSubTitle(instructionsView));
  EXPECT_TRUE(HasInstructionSteps(instructionsView));
  EXPECT_TRUE(HasPrimaryActionButton(instructionsView));
  EXPECT_FALSE(HasSecondaryActionButton(instructionsView));
  EXPECT_FALSE(HasTertiaryActionButton(instructionsView));
}

// Test view creation with secondary button.
TEST_F(DefaultBrowserInstructionsViewTest, CreateViewWithSecondaryButtonTest) {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] initWithDismissButton:YES
                                                   hasRemindMeLater:NO
                                                           hasSteps:NO
                                                      actionHandler:nil
                                          alertScreenViewController:alertScreen
                                                          titleText:nil];
  ASSERT_NE(instructionsView, nil);
  EXPECT_TRUE(HasTitle(instructionsView));
  EXPECT_TRUE(HasSubTitle(instructionsView));
  EXPECT_FALSE(HasInstructionSteps(instructionsView));
  EXPECT_TRUE(HasPrimaryActionButton(instructionsView));
  EXPECT_TRUE(HasSecondaryActionButton(instructionsView));
  EXPECT_FALSE(HasTertiaryActionButton(instructionsView));
}

// Test view creation with tertiary button.
TEST_F(DefaultBrowserInstructionsViewTest, CreateViewWithTertiaryButtonTest) {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] initWithDismissButton:NO
                                                   hasRemindMeLater:YES
                                                           hasSteps:NO
                                                      actionHandler:nil
                                          alertScreenViewController:alertScreen
                                                          titleText:nil];
  ASSERT_NE(instructionsView, nil);
  EXPECT_TRUE(HasTitle(instructionsView));
  EXPECT_TRUE(HasSubTitle(instructionsView));
  EXPECT_FALSE(HasInstructionSteps(instructionsView));
  EXPECT_TRUE(HasPrimaryActionButton(instructionsView));
  EXPECT_FALSE(HasSecondaryActionButton(instructionsView));
  EXPECT_TRUE(HasTertiaryActionButton(instructionsView));
}

// Test the animation view.
TEST_F(DefaultBrowserInstructionsViewTest, AnimationViewTest) {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];
  DefaultBrowserInstructionsView* instructionsView =
      [[DefaultBrowserInstructionsView alloc] initWithDismissButton:YES
                                                   hasRemindMeLater:NO
                                                           hasSteps:NO
                                                      actionHandler:nil
                                          alertScreenViewController:alertScreen
                                                          titleText:nil];
  ASSERT_NE(instructionsView, nil);

  UIView* animationView = GetAnimationSubview(instructionsView);
  UIView* darkAnimationView = GetDarkAnimationSubview(instructionsView);
  EXPECT_NE(animationView, nil);
  EXPECT_NE(darkAnimationView, nil);

  EXPECT_FALSE(animationView.hidden);
  EXPECT_TRUE(darkAnimationView.hidden);
}
