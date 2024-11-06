// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view_controller.h"

#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using DefaultBrowserInstructionsViewControllerTest = PlatformTest;

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
TEST_F(DefaultBrowserInstructionsViewControllerTest,
       CreateViewWithSubtitleTest) {
  DefaultBrowserInstructionsViewController* instructionsViewController =
      [[DefaultBrowserInstructionsViewController alloc]
          initWithDismissButton:NO
               hasRemindMeLater:NO
                       hasSteps:NO
                  actionHandler:nil
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
                       hasSteps:YES
                  actionHandler:nil
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
                       hasSteps:NO
                  actionHandler:nil
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
                       hasSteps:NO
                  actionHandler:nil
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
                       hasSteps:NO
                  actionHandler:nil
                      titleText:nil];
  UIView* view = instructionsViewController.view;
  ASSERT_NE(instructionsViewController, nil);

  UIView* animationView = GetAnimationSubview(view);
  UIView* darkAnimationView = GetDarkAnimationSubview(view);
  EXPECT_NE(animationView, nil);
  EXPECT_NE(darkAnimationView, nil);

  EXPECT_FALSE(animationView.hidden);
  EXPECT_TRUE(darkAnimationView.hidden);
}
