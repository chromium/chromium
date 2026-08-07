// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/wallet_reminder_notice/ui/wallet_reminder_notice_view_controller.h"

#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface FakeWalletReminderNoticeViewControllerDelegate
    : NSObject <WalletReminderNoticeViewControllerDelegate>
@property(nonatomic, assign) BOOL didTapPrimaryActionCalled;
@property(nonatomic, strong) NSURL* tappedURL;
@end

@implementation FakeWalletReminderNoticeViewControllerDelegate
- (void)walletReminderNoticeViewControllerDidTapPrimaryAction:
    (WalletReminderNoticeViewController*)viewController {
  self.didTapPrimaryActionCalled = YES;
}

- (void)walletReminderNoticeViewController:
            (WalletReminderNoticeViewController*)viewController
                             didTapLinkURL:(NSURL*)URL {
  self.tappedURL = URL;
}
@end

using WalletReminderNoticeViewControllerTest = PlatformTest;

TEST_F(WalletReminderNoticeViewControllerTest, SetConsumerProperties) {
  WalletReminderNoticeViewController* view_controller =
      [[WalletReminderNoticeViewController alloc] init];

  NSString* title =
      @"Your Google Wallet settings apply to info saved from Chrome";
  NSString* primaryAction = @"Got it";

  [view_controller setTitleString:title];
  [view_controller setPrimaryActionString:primaryAction];

  [view_controller loadViewIfNeeded];

  EXPECT_NSEQ(title, view_controller.titleString);
  EXPECT_NSEQ(primaryAction, view_controller.configuration.primaryActionString);
}

TEST_F(WalletReminderNoticeViewControllerTest, SetDisclaimerText) {
  WalletReminderNoticeViewController* view_controller =
      [[WalletReminderNoticeViewController alloc] init];

  SaveCardMessageWithLinks* message = [[SaveCardMessageWithLinks alloc] init];
  message.messageText = @"Test legal disclaimer message";

  [view_controller setDisclaimerText:@[ message ]];

  EXPECT_NE(nil, view_controller.underTitleView);
}

TEST_F(WalletReminderNoticeViewControllerTest, PrimaryActionNotifiesDelegate) {
  WalletReminderNoticeViewController* view_controller =
      [[WalletReminderNoticeViewController alloc] init];
  FakeWalletReminderNoticeViewControllerDelegate* delegate =
      [[FakeWalletReminderNoticeViewControllerDelegate alloc] init];
  view_controller.delegate = delegate;

  [(id<ConfirmationAlertActionHandler>)
          view_controller confirmationAlertPrimaryAction];

  EXPECT_TRUE(delegate.didTapPrimaryActionCalled);
}

TEST_F(WalletReminderNoticeViewControllerTest, LinkTapNotifiesDelegate) {
  WalletReminderNoticeViewController* view_controller =
      [[WalletReminderNoticeViewController alloc] init];
  FakeWalletReminderNoticeViewControllerDelegate* delegate =
      [[FakeWalletReminderNoticeViewControllerDelegate alloc] init];
  view_controller.delegate = delegate;

  NSURL* test_url = [NSURL URLWithString:@"https://wallet.google.com/settings"];
  id text_item = OCMClassMock([UITextItem class]);
  OCMStub([text_item link]).andReturn(test_url);

  UIAction* default_action = [UIAction actionWithHandler:^(UIAction*){
  }];
  UIAction* action = [(id<UITextViewDelegate>)view_controller
                      textView:[[UITextView alloc] init]
      primaryActionForTextItem:text_item
                 defaultAction:default_action];
  EXPECT_NE(nil, action);
}
