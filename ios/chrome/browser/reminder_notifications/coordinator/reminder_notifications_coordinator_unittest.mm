// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reminder_notifications_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

#pragma mark - Test Fixture

class ReminderNotificationsCoordinatorTest : public PlatformTest {
 protected:
  ReminderNotificationsCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];

    // Set up mock command handler
    mock_reminder_notifications_handler_ =
        OCMStrictProtocolMock(@protocol(ReminderNotificationsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_reminder_notifications_handler_
                     forProtocol:@protocol(ReminderNotificationsCommands)];
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  ReminderNotificationsCoordinator* CreateCoordinator() {
    coordinator_ = [[ReminderNotificationsCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()];
    return coordinator_;
  }

  // Helper to add a `FakeWebState` with a specific URL.
  void AddTestWebState(const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetCurrentURL(url);
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ReminderNotificationsCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
  id mock_reminder_notifications_handler_;
};

#pragma mark - Tests

// Tests that the coordinator presents a view controller when started.
TEST_F(ReminderNotificationsCoordinatorTest,
       PresentsViewControllerWhenStarted) {
  CreateCoordinator();
  EXPECT_EQ(nil, view_controller_.presentedViewController);

  [coordinator_ start];

  EXPECT_TRUE([view_controller_.presentedViewController
      isKindOfClass:[ReminderNotificationsViewController class]]);
}

// Tests that the coordinator configures the presentation style correctly.
TEST_F(ReminderNotificationsCoordinatorTest,
       ConfiguresHalfSheetPresentationStyle) {
  CreateCoordinator();
  [coordinator_ start];

  UIViewController* presented_view_controller =
      view_controller_.presentedViewController;

  UISheetPresentationController* sheet_controller =
      presented_view_controller.sheetPresentationController;
  EXPECT_TRUE(sheet_controller.prefersEdgeAttachedInCompactHeight);
  EXPECT_EQ(2u, sheet_controller.detents.count);
}

// Tests that tapping the primary action dismisses the UI and sends the command.
TEST_F(ReminderNotificationsCoordinatorTest,
       DismissesUIAndNotifiesOnPrimaryAction) {
  CreateCoordinator();

  GURL test_url("https://valid.example.com");
  AddTestWebState(test_url);

  [coordinator_ start];

  OCMExpect([mock_reminder_notifications_handler_ dismissSetTabReminderUI]);

  [(id<ConfirmationAlertActionHandler>)
          coordinator_ confirmationAlertPrimaryAction];

  EXPECT_OCMOCK_VERIFY(mock_reminder_notifications_handler_);
}

// Tests that tapping the secondary action dismisses the UI and sends the
// command.
TEST_F(ReminderNotificationsCoordinatorTest,
       DismissesUIAndNotifiesOnSecondaryAction) {
  CreateCoordinator();
  [coordinator_ start];

  OCMExpect([mock_reminder_notifications_handler_ dismissSetTabReminderUI]);

  [(id<ConfirmationAlertActionHandler>)
          coordinator_ confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(mock_reminder_notifications_handler_);
}

// Tests that dismissing via the presentation controller sends the command.
TEST_F(ReminderNotificationsCoordinatorTest,
       NotifiesWhenDismissedViaPresentationController) {
  CreateCoordinator();
  [coordinator_ start];

  OCMExpect([mock_reminder_notifications_handler_ dismissSetTabReminderUI]);

  [(id<UIAdaptivePresentationControllerDelegate>)coordinator_
      presentationControllerDidDismiss:view_controller_.presentedViewController
                                           .presentationController];

  EXPECT_OCMOCK_VERIFY(mock_reminder_notifications_handler_);
}
