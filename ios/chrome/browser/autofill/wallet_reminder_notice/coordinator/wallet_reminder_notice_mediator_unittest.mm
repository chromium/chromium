// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/wallet_reminder_notice/coordinator/wallet_reminder_notice_mediator.h"

#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/wallet_reminder_notice/ui/wallet_reminder_notice_consumer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

@interface FakeWalletReminderNoticeConsumer
    : NSObject <WalletReminderNoticeConsumer>

@property(nonatomic, copy) NSString* titleString;
@property(nonatomic, copy) NSString* primaryActionString;
@property(nonatomic, strong) NSArray<SaveCardMessageWithLinks*>* disclaimerText;

@end

@implementation FakeWalletReminderNoticeConsumer
@end

class WalletReminderNoticeMediatorTest : public PlatformTest {
 protected:
  WalletReminderNoticeMediatorTest() = default;
};

TEST_F(WalletReminderNoticeMediatorTest, SetsConsumerData) {
  autofill::LegalMessageLines legal_message_lines;
  WalletReminderNoticeMediator* mediator = [[WalletReminderNoticeMediator alloc]
      initWithLegalMessageLines:legal_message_lines];

  FakeWalletReminderNoticeConsumer* consumer =
      [[FakeWalletReminderNoticeConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_AUTOFILL_WALLET_REMINDER_NOTICE_TITLE),
              consumer.titleString);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_AUTOFILL_WALLET_REMINDER_NOTICE_CONFIRM_BUTTON_LABEL),
              consumer.primaryActionString);
}

TEST_F(WalletReminderNoticeMediatorTest, SetsLegalMessageLines) {
  autofill::LegalMessageLines legal_message_lines = {
      autofill::TestLegalMessageLine("Test legal message line")};

  WalletReminderNoticeMediator* mediator = [[WalletReminderNoticeMediator alloc]
      initWithLegalMessageLines:legal_message_lines];

  FakeWalletReminderNoticeConsumer* consumer =
      [[FakeWalletReminderNoticeConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(1U, consumer.disclaimerText.count);
  EXPECT_NSEQ(@"Test legal message line",
              consumer.disclaimerText[0].messageText);
}
