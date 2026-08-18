// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_coordinator_delegate.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_params.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_user_action.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake delegate that records the dismiss action.
@interface FakeBackendPromoCustomUICoordinatorDelegate
    : NSObject <BackendPromoCustomUICoordinatorDelegate>
@property(nonatomic, assign) BOOL delegateCalled;
@property(nonatomic, assign) BackendPromoUserAction userAction;
@end

@implementation FakeBackendPromoCustomUICoordinatorDelegate

- (void)backendPromoCustomUICoordinator:
            (BackendPromoCustomUICoordinator*)coordinator
                   didDismissWithAction:(BackendPromoUserAction)action {
  self.delegateCalled = YES;
  self.userAction = action;
}

@end

class BackendPromoCustomUICoordinatorTest : public PlatformTest {
 protected:
  BackendPromoCustomUICoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    delegate_ = [[FakeBackendPromoCustomUICoordinatorDelegate alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  FakeBackendPromoCustomUICoordinatorDelegate* delegate_;
};

// Tests that coordinator start presents ConfirmationAlertViewController and
// stop dismisses it.
TEST_F(BackendPromoCustomUICoordinatorTest, TestStartAndStop) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";
  params.primaryActionTitle = @"Primary Action";
  params.secondaryActionTitle = @"Secondary Action";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[ConfirmationAlertViewController class]]);

  ConfirmationAlertViewController* view_controller =
      (ConfirmationAlertViewController*)
          base_view_controller_.presentedViewController;
  EXPECT_NSEQ(view_controller.titleString, @"Test Title");
  EXPECT_NSEQ(view_controller.subtitleString, @"Test Body");
  EXPECT_NSEQ(view_controller.configuration.primaryActionString,
              @"Primary Action");
  EXPECT_NSEQ(view_controller.configuration.secondaryActionString,
              @"Secondary Action");

  [coordinator stop];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController == nil;
      }));
}

// Tests that primary action invokes delegate.
TEST_F(BackendPromoCustomUICoordinatorTest, TestPrimaryActionTriggersDelegate) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ConfirmationAlertViewController* view_controller =
      (ConfirmationAlertViewController*)
          base_view_controller_.presentedViewController;

  [view_controller.actionHandler confirmationAlertPrimaryAction];

  EXPECT_TRUE(delegate_.delegateCalled);
  EXPECT_EQ(delegate_.userAction, BackendPromoUserActionAccepted);
}

// Tests that secondary action invokes delegate.
TEST_F(BackendPromoCustomUICoordinatorTest,
       TestSecondaryActionTriggersDelegate) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ConfirmationAlertViewController* view_controller =
      (ConfirmationAlertViewController*)
          base_view_controller_.presentedViewController;

  [view_controller.actionHandler confirmationAlertSecondaryAction];

  EXPECT_TRUE(delegate_.delegateCalled);
  EXPECT_EQ(delegate_.userAction, BackendPromoUserActionDismissed);
}

// Tests that confirmation alert dismissed invokes delegate.
TEST_F(BackendPromoCustomUICoordinatorTest,
       TestDismissedActionTriggersDelegate) {
  BackendPromoCustomUIParams* params =
      [[BackendPromoCustomUIParams alloc] init];
  params.title = @"Test Title";
  params.body = @"Test Body";

  BackendPromoCustomUICoordinator* coordinator =
      [[BackendPromoCustomUICoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              params:params];
  coordinator.delegate = delegate_;

  [coordinator start];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  ConfirmationAlertViewController* view_controller =
      (ConfirmationAlertViewController*)
          base_view_controller_.presentedViewController;

  [view_controller.actionHandler confirmationAlertDismissed];

  EXPECT_TRUE(delegate_.delegateCalled);
  EXPECT_EQ(delegate_.userAction, BackendPromoUserActionCancelled);
}
