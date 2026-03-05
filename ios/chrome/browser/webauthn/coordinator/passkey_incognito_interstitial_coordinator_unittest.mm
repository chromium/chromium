// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/passkey_incognito_interstitial_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/webauthn/ui/passkey_incognito_interstitial_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

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
  base::test::TestFuture<bool> test_future;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:test_future.GetCallback()];

  [coordinator_ start];

  id<ConfirmationAlertActionHandler> action_handler =
      (id<ConfirmationAlertActionHandler>)coordinator_;

  [action_handler confirmationAlertPrimaryAction];

  [coordinator_ stop];

  EXPECT_TRUE(test_future.Get());
}

// Tests that tapping the 'Cancel' button runs the callback with `false`.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest,
       SecondaryActionReturnsFalse) {
  base::test::TestFuture<bool> test_future;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:test_future.GetCallback()];

  [coordinator_ start];

  id<ConfirmationAlertActionHandler> action_handler =
      (id<ConfirmationAlertActionHandler>)coordinator_;

  [action_handler confirmationAlertSecondaryAction];

  [coordinator_ stop];

  EXPECT_FALSE(test_future.Get());
}

// Tests that forcefully stopping the coordinator runs the callback with
// `false`.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest,
       StopReturnsFalseIfNoActionTaken) {
  base::test::TestFuture<bool> test_future;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:test_future.GetCallback()];

  [coordinator_ start];
  [coordinator_ stop];

  EXPECT_FALSE(test_future.Get());
}

// Tests that dismissing the sheet runs the callback with `false`.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest, SwipeDownReturnsFalse) {
  base::test::TestFuture<bool> test_future;

  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:test_future.GetCallback()];

  [coordinator_ start];

  id<PasskeyIncognitoInterstitialViewControllerDelegate> delegate =
      (id<PasskeyIncognitoInterstitialViewControllerDelegate>)coordinator_;

  [delegate passkeyIncognitoInterstitialViewDidDisappear];

  EXPECT_FALSE(test_future.Get());
}

// Tests that the callback is strictly executed only once.
TEST_F(PasskeyIncognitoInterstitialCoordinatorTest, CallbackNotCalledTwice) {
  base::test::TestFuture<bool> test_future;
  coordinator_ = [[PasskeyIncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        callback:test_future.GetCallback()];
  [coordinator_ start];

  id<PasskeyIncognitoInterstitialViewControllerDelegate> delegate =
      (id<PasskeyIncognitoInterstitialViewControllerDelegate>)coordinator_;
  [delegate passkeyIncognitoInterstitialViewDidDisappear];

  EXPECT_TRUE(test_future.IsReady());
  EXPECT_FALSE(test_future.Get());

  [delegate passkeyIncognitoInterstitialViewDidDisappear];
}
