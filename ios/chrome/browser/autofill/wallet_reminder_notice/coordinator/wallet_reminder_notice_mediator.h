// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_COORDINATOR_WALLET_REMINDER_NOTICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_COORDINATOR_WALLET_REMINDER_NOTICE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/browser/payments/legal_message_line.h"

@protocol WalletReminderNoticeConsumer;

// This mediator updates the Wallet Reminder Notice view controller.
@interface WalletReminderNoticeMediator : NSObject

// Consumer interface for updating the Wallet Reminder Notice bottom sheet.
@property(nonatomic, weak) id<WalletReminderNoticeConsumer> consumer;

// Initializes this mediator with the provided legal message lines.
- (instancetype)initWithLegalMessageLines:
    (autofill::LegalMessageLines)legalMessageLines NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_COORDINATOR_WALLET_REMINDER_NOTICE_MEDIATOR_H_
