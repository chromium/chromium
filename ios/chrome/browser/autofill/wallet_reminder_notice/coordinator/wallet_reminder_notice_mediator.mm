// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/wallet_reminder_notice/coordinator/wallet_reminder_notice_mediator.h"

#import <utility>

#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/wallet_reminder_notice/ui/wallet_reminder_notice_consumer.h"
#import "ui/base/l10n/l10n_util.h"

@implementation WalletReminderNoticeMediator {
  autofill::LegalMessageLines _legalMessageLines;
}

- (instancetype)initWithLegalMessageLines:
    (autofill::LegalMessageLines)legalMessageLines {
  self = [super init];
  if (self) {
    _legalMessageLines = std::move(legalMessageLines);
  }
  return self;
}

#pragma mark - Properties

- (void)setConsumer:(id<WalletReminderNoticeConsumer>)consumer {
  _consumer = consumer;
  if (!_consumer) {
    return;
  }

  [_consumer setTitleString:l10n_util::GetNSString(
                                IDS_AUTOFILL_WALLET_REMINDER_NOTICE_TITLE)];
  [_consumer setPrimaryActionString:
                 l10n_util::GetNSString(
                     IDS_AUTOFILL_WALLET_REMINDER_NOTICE_CONFIRM_BUTTON_LABEL)];

  if (!_legalMessageLines.empty()) {
    [_consumer setDisclaimerText:[SaveCardMessageWithLinks
                                     convertFrom:_legalMessageLines]];
  }
}

@end
