// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/ui/default_browser_instructions_view_controller.h"

#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

using DefaultBrowserInstructionsViewControllerTest = PlatformTest;

namespace {

UIView* GetAnimationSubview(UIView* view) {
  return chrome_test_util::FindViewById(
      view, kDefaultBrowserInstructionsViewAnimationViewId);
}

UIView* GetDarkAnimationSubview(UIView* view) {
  return chrome_test_util::FindViewById(
      view, kDefaultBrowserInstructionsViewDarkAnimationViewId);
}

bool HasTitle(UIView* view) {
  return chrome_test_util::FindViewById(
             view, kConfirmationAlertTitleAccessibilityIdentifier) != nil;
}

bool HasSubTitle(UIView* view) {
  return chrome_test_util::FindViewById(
             view, kConfirmationAlertSubtitleAccessibilityIdentifier) != nil;
}

bool HasInstructionSteps(UIView* view) {
  return chrome_test_util::FindViewById(
             view, kConfirmationAlertUnderTitleViewAccessibilityIdentifier) !=
         nil;
}

bool HasPrimaryActionButton(UIView* view) {
  UIView* button = chrome_test_util::FindViewById(
      view, kButtonStackPrimaryActionAccessibilityIdentifier);
  return button && !button.hidden;
}

bool HasSecondaryActionButton(UIView* view) {
  UIView* button = chrome_test_util::FindViewById(
      view, kButtonStackSecondaryActionAccessibilityIdentifier);
  return button && !button.hidden;
}

bool HasTertiaryActionButton(UIView* view) {
  UIView* button = chrome_test_util::FindViewById(
      view, kButtonStackTertiaryActionAccessibilityIdentifier);
  return button && !button.hidden;
}

}  // namespace

// Test view creation with subtitle.
TEST_F(DefaultBrowserInstructionsViewControllerTest,
       CreateViewWithSubtitleTest) {
  DefaultBrowserInstructionsViewController* instructionsViewController =
      [[DefaultBrowserInstructionsViewController alloc]
              initWithDismissButton:NO
                   hasRemindMeLater:NO
          useDefaultAppsDestination:NO
                           hasSteps:NO
                          titleText:nil];
  UIView* view = instructionsViewController.view;
  ASSERT_NE(instructionsViewController, nil);
  EXPECT_TRUE(HasTitle(view));
  EXPECT_TRUE(HasSubTitle(view));
  EXPECT_FALSE(HasInstructionSteps(view));
  EXPECT_TRUE(HasPrimaryActionButton(view));
  EXPECT_FALSE(HasSecondaryActionButton(view));
  EXPECT_FALSE(HasTertiaryActionButton(view));
}

// Test view creation with instruction steps.
TEST_F(DefaultBrowserInstructionsViewControllerTest, CreateViewWithStepsTest) {
  DefaultBrowserInstructionsViewController* instructionsViewController =
      [[DefaultBrowserInstructionsViewController alloc]
              initWithDismissButton:NO
                   hasRemindMeLater:NO
          useDefaultAppsDestination:NO
                           hasSteps:YES
                          titleText:nil];
  UIView* view = instructionsViewController.view;
  ASSERT_NE(instructionsViewController, nil);
  EXPECT_TRUE(HasTitle(view));
  EXPECT_FALSE(HasSubTitle(view));
  EXPECT_TRUE(HasInstructionSteps(view));
  EXPECT_TRUE(HasPrimaryActionButton(view));
  EXPECT_FALSE(HasSecondaryActionButton(view));
  EXPECT_FALSE(HasTertiaryActionButton(view));
}

// Test view creation with secondary button.
TEST_F(DefaultBrowserInstructionsViewControllerTest,
       CreateViewWithSecondaryButtonTest) {
  DefaultBrowserInstructionsViewController* instructionsViewController =
      [[DefaultBrowserInstructionsViewController alloc]
              initWithDismissButton:YES
                   hasRemindMeLater:NO
          useDefaultAppsDestination:NO
                           hasSteps:NO
                          titleText:nil];
  UIView* view = instructionsViewController.view;
  ASSERT_NE(instructionsViewController, nil);
  EXPECT_TRUE(HasTitle(view));
  EXPECT_TRUE(HasSubTitle(view));
  EXPECT_FALSE(HasInstructionSteps(view));
  EXPECT_TRUE(HasPrimaryActionButton(view));
  EXPECT_TRUE(HasSecondaryActionButton(view));
  EXPECT_FALSE(HasTertiaryActionButton(view));
}

// Test view creation with tertiary button.
TEST_F(DefaultBrowserInstructionsViewControllerTest,
       CreateViewWithTertiaryButtonTest) {
  DefaultBrowserInstructionsViewController* instructionsViewController =
      [[DefaultBrowserInstructionsViewController alloc]
              initWithDismissButton:NO
                   hasRemindMeLater:YES
          useDefaultAppsDestination:NO
                           hasSteps:NO
                          titleText:nil];
  UIView* view = instructionsViewController.view;
  ASSERT_NE(instructionsViewController, nil);
  EXPECT_TRUE(HasTitle(view));
  EXPECT_TRUE(HasSubTitle(view));
  EXPECT_FALSE(HasInstructionSteps(view));
  EXPECT_TRUE(HasPrimaryActionButton(view));
  EXPECT_FALSE(HasSecondaryActionButton(view));
  EXPECT_TRUE(HasTertiaryActionButton(view));
}

// Test the animation view.
TEST_F(DefaultBrowserInstructionsViewControllerTest, AnimationViewTest) {
  DefaultBrowserInstructionsViewController* instructionsViewController =
      [[DefaultBrowserInstructionsViewController alloc]
              initWithDismissButton:YES
                   hasRemindMeLater:NO
          useDefaultAppsDestination:NO
                           hasSteps:NO
                          titleText:nil];
  UIView* view = instructionsViewController.view;
  ASSERT_NE(instructionsViewController, nil);

  UIView* animationView = GetAnimationSubview(view);
  UIView* darkAnimationView = GetDarkAnimationSubview(view);

  BOOL isiPad = IsDefaultBrowserPromoIpadInstructions() &&
                ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;

  // The iPad video uses one dynamic lottie animation for dark mode.
  EXPECT_NE(animationView, nil);
  if (isiPad) {
    EXPECT_EQ(darkAnimationView, nil);
  } else {
    EXPECT_NE(darkAnimationView, nil);
  }

  EXPECT_FALSE(animationView.hidden);
  EXPECT_EQ(darkAnimationView.hidden, !isiPad);
}
