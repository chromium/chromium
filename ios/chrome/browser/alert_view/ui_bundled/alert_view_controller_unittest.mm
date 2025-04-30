// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "testing/platform_test.h"

using AlertViewControllerTest = PlatformTest;

// Tests AlertViewController can be initiliazed.
TEST_F(AlertViewControllerTest, Init) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  EXPECT_TRUE(alert);
}

// Tests there are no circular references in a simple init.
TEST_F(AlertViewControllerTest, Dealloc) {
  __weak AlertViewController* weakAlert = nil;
  @autoreleasepool {
    AlertViewController* alert = [[AlertViewController alloc] init];
    weakAlert = alert;
  }
  EXPECT_FALSE(weakAlert);
}

// Tests there are no circular references in an alert with actions.
TEST_F(AlertViewControllerTest, DeallocWithActions) {
  __weak AlertViewController* weakAlert = nil;
  @autoreleasepool {
    AlertViewController* alert = [[AlertViewController alloc] init];
    AlertAction* action =
        [AlertAction actionWithTitle:@"OK"
                               style:UIAlertActionStyleDefault
                             handler:^(AlertAction* alert_action){
                             }];
    [alert setActions:@[ @[ action ] ]];
    weakAlert = alert;
  }
  EXPECT_FALSE(weakAlert);
}

namespace {
UIActivityIndicatorView* GetSpinner(AlertViewController* controller) {
  return [controller valueForKey:@"_spinner"];
}

UIImageView* GetCheckmark(AlertViewController* controller) {
  return [controller valueForKey:@"_checkmark"];
}
}  // namespace

// Tests that the Activity state is set correctly after view loads.
TEST_F(AlertViewControllerTest, ProgressState_ActivityIsSetByLoadView) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  [alert setValue:@YES forKey:@"_shouldShowActivityIndicator"];

  (void)alert.view;

  UIActivityIndicatorView* spinner = GetSpinner(alert);
  UIImageView* checkmark = GetCheckmark(alert);

  ASSERT_TRUE(spinner);
  ASSERT_TRUE(checkmark);
  EXPECT_FALSE(spinner.hidden);
  EXPECT_TRUE(spinner.isAnimating);
  EXPECT_TRUE(checkmark.hidden);
}

// Tests setting state to Success updates views correctly.
TEST_F(AlertViewControllerTest, ProgressState_SetSuccess) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  [alert setValue:@YES forKey:@"_shouldShowActivityIndicator"];

  (void)alert.view;

  // Set state to Success
  [alert setProgressState:ProgressIndicatorStateSuccess];
  UIActivityIndicatorView* spinner = GetSpinner(alert);
  UIImageView* checkmark = GetCheckmark(alert);

  // Verify Success state
  ASSERT_TRUE(spinner);
  ASSERT_TRUE(checkmark);
  EXPECT_TRUE(spinner.hidden);
  EXPECT_FALSE(spinner.isAnimating);
  EXPECT_FALSE(checkmark.hidden);
}

// Tests setting state to None updates views correctly.
TEST_F(AlertViewControllerTest, ProgressState_SetNone) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  [alert setValue:@YES forKey:@"_shouldShowActivityIndicator"];

  (void)alert.view;

  // Set state to None
  [alert setProgressState:ProgressIndicatorStateNone];
  UIActivityIndicatorView* spinner = GetSpinner(alert);
  UIImageView* checkmark = GetCheckmark(alert);

  // Verify None state
  ASSERT_TRUE(spinner);
  ASSERT_TRUE(checkmark);
  EXPECT_TRUE(spinner.hidden);
  EXPECT_FALSE(spinner.isAnimating);
  EXPECT_TRUE(checkmark.hidden);
}

// Tests setting state to the current state does not cause changes.
TEST_F(AlertViewControllerTest, ProgressState_SetSameState) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  [alert setValue:@YES forKey:@"_shouldShowActivityIndicator"];

  (void)alert.view;
  UIActivityIndicatorView* spinner = GetSpinner(alert);
  UIImageView* checkmark = GetCheckmark(alert);

  // Capture initial state
  BOOL initialSpinnerHidden = spinner.hidden;
  BOOL initialSpinnerAnimating = spinner.isAnimating;
  BOOL initialCheckmarkHidden = checkmark.hidden;

  // Set state to the same state
  [alert setProgressState:ProgressIndicatorStateActivity];

  // Verify state hasn't visually changed
  EXPECT_EQ(initialSpinnerHidden, spinner.hidden);
  EXPECT_EQ(initialSpinnerAnimating, spinner.isAnimating);
  EXPECT_EQ(initialCheckmarkHidden, checkmark.hidden);
}

// Tests that if shouldShowActivityIndicator is NO, views are not updated.
TEST_F(AlertViewControllerTest, ProgressState_IndicatorNotSupported) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  [alert setValue:@NO forKey:@"_shouldShowActivityIndicator"];

  (void)alert.view;

  // Verify views were not created
  EXPECT_FALSE(GetSpinner(alert));
  EXPECT_FALSE(GetCheckmark(alert));

  [alert setProgressState:ProgressIndicatorStateSuccess];

  // Verify internal state updated, but no views to affect
  EXPECT_EQ(ProgressIndicatorStateSuccess, alert.progressState);
}

// Tests that setting state before view loads applies correctly after load.
TEST_F(AlertViewControllerTest, ProgressState_SetStateBeforeViewLoad) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  [alert setValue:@YES forKey:@"_shouldShowActivityIndicator"];

  // Verify internal state is set
  EXPECT_EQ(ProgressIndicatorStateNone, alert.progressState);

  // Verify views don't exist yet
  EXPECT_FALSE([alert valueForKey:@"_spinner"]);
  EXPECT_FALSE([alert valueForKey:@"_checkmark"]);

  // Trigger view loading (which calls viewDidLoad, applying the state)
  (void)alert.view;
  UIActivityIndicatorView* spinner = GetSpinner(alert);
  UIImageView* checkmark = GetCheckmark(alert);

  // Verify Activity state is now applied visually
  ASSERT_TRUE(spinner);
  ASSERT_TRUE(checkmark);
  EXPECT_FALSE(spinner.hidden);
  EXPECT_TRUE(spinner.isAnimating);
  EXPECT_TRUE(checkmark.hidden);
}
