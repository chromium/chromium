// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/passkey_incognito_interstitial_coordinator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/webauthn/ui/passkey_incognito_interstitial_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class PasskeyIncognitoInterstitialCoordinatorTest : public PlatformTest {
 protected:
  PasskeyIncognitoInterstitialCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  ScopedKeyWindow scoped_key_window_;
  PasskeyIncognitoInterstitialCoordinator* coordinator_;
};

// Tests that tapping the 'Continue' button runs the callback with `true`.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest, PrimaryActionReturnsTrue) {
  __block bool callback_executed = false;
  __block bool callback_result = false;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:base::BindOnce(^(bool proceed) {
                          callback_executed = true;
                          callback_result = proceed;
                        })];

  [coordinator_ start];

  id<ConfirmationAlertActionHandler> action_handler =
      (id<ConfirmationAlertActionHandler>)coordinator_;

  [action_handler confirmationAlertPrimaryAction];

  EXPECT_TRUE(callback_executed);
  EXPECT_TRUE(callback_result);
}

// Tests that tapping the 'Cancel' button runs the callback with `false`.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest,
       SecondaryActionReturnsFalse) {
  __block bool callback_executed = false;
  __block bool callback_result = true;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:base::BindOnce(^(bool proceed) {
                          callback_executed = true;
                          callback_result = proceed;
                        })];

  [coordinator_ start];

  id<ConfirmationAlertActionHandler> action_handler =
      (id<ConfirmationAlertActionHandler>)coordinator_;

  [action_handler confirmationAlertSecondaryAction];

  EXPECT_TRUE(callback_executed);
  EXPECT_FALSE(callback_result);
}

// Tests that forcefully stopping the coordinator runs the callback with
// `false`.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest,
       StopReturnsFalseIfNoActionTaken) {
  __block bool callback_executed = false;
  __block bool callback_result = true;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:base::BindOnce(^(bool proceed) {
                          callback_executed = true;
                          callback_result = proceed;
                        })];

  [coordinator_ start];

  [coordinator_ stop];

  EXPECT_TRUE(callback_executed);
  EXPECT_FALSE(callback_result);
}

// Tests that dismissing the sheet runs the callback with `false`.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest, SwipeDownReturnsFalse) {
  __block bool callback_executed = false;
  __block bool callback_result = true;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:base::BindOnce(^(bool proceed) {
                          callback_executed = true;
                          callback_result = proceed;
                        })];

  [coordinator_ start];

  id<PasskeyIncognitoInterstitialViewControllerDelegate> delegate =
      (id<PasskeyIncognitoInterstitialViewControllerDelegate>)coordinator_;

  [delegate passkeyIncognitoInterstitialViewDidDisappear];

  EXPECT_TRUE(callback_executed);
  EXPECT_FALSE(callback_result);
}

// Tests that the callback is strictly executed only once.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest, CallbackNotCalledTwice) {
  __block int callback_execution_count = 0;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:base::BindOnce(^(bool proceed) {
                          callback_execution_count++;
                        })];

  [coordinator_ start];

  id<ConfirmationAlertActionHandler> action_handler =
      (id<ConfirmationAlertActionHandler>)coordinator_;

  [action_handler confirmationAlertPrimaryAction];

  [coordinator_ stop];

  id<PasskeyIncognitoInterstitialViewControllerDelegate> delegate =
      (id<PasskeyIncognitoInterstitialViewControllerDelegate>)coordinator_;
  [delegate passkeyIncognitoInterstitialViewDidDisappear];

  EXPECT_EQ(1, callback_execution_count);
}
