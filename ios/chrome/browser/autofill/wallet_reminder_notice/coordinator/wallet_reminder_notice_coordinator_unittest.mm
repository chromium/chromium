// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/wallet_reminder_notice/coordinator/wallet_reminder_notice_coordinator.h"

#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "ios/chrome/browser/autofill/wallet_reminder_notice/ui/wallet_reminder_notice_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class WalletReminderNoticeCoordinatorTest : public PlatformTest {
 protected:
  WalletReminderNoticeCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
};

// Tests starting and stopping the coordinator.
TEST_F(WalletReminderNoticeCoordinatorTest, StartAndStop) {
  autofill::LegalMessageLines legal_message_lines;
  WalletReminderNoticeCoordinator* coordinator =
      [[WalletReminderNoticeCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                   legalMessageLines:legal_message_lines];

  [coordinator start];
  [coordinator stop];
}

// Tests that tapping the primary action button (e.g., "Got it") dismisses the
// notice.
TEST_F(WalletReminderNoticeCoordinatorTest, PrimaryActionDismissesNotice) {
  autofill::LegalMessageLines legal_message_lines;
  WalletReminderNoticeCoordinator* coordinator =
      [[WalletReminderNoticeCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                   legalMessageLines:legal_message_lines];

  [coordinator start];

  id<ConfirmationAlertActionHandler> handler =
      static_cast<id<ConfirmationAlertActionHandler>>(coordinator);
  [handler confirmationAlertPrimaryAction];
}

// Tests that tapping a legal link dispatches an OpenNewTabCommand to
// SceneCommands.
TEST_F(WalletReminderNoticeCoordinatorTest,
       LinkTapDispatchesOpenNewTabCommand) {
  autofill::LegalMessageLines legal_message_lines;
  WalletReminderNoticeCoordinator* coordinator =
      [[WalletReminderNoticeCoordinator alloc]
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                   legalMessageLines:legal_message_lines];

  id mock_scene_handler = OCMProtocolMock(@protocol(SceneCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_scene_handler
                   forProtocol:@protocol(SceneCommands)];

  [coordinator start];

  NSURL* test_url = [NSURL URLWithString:@"https://wallet.google.com/settings"];
  OCMExpect([mock_scene_handler
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        return command.URL == net::GURLWithNSURL(test_url);
      }]]);

  id<WalletReminderNoticeViewControllerDelegate> delegate =
      static_cast<id<WalletReminderNoticeViewControllerDelegate>>(coordinator);
  [delegate walletReminderNoticeViewController:nil didTapLinkURL:test_url];

  EXPECT_OCMOCK_VERIFY(mock_scene_handler);
}
