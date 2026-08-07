// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_COORDINATOR_WALLET_REMINDER_NOTICE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_COORDINATOR_WALLET_REMINDER_NOTICE_COORDINATOR_H_

#include "components/autofill/core/browser/payments/legal_message_line.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator responsible for creating and displaying the Wallet Reminder
// Notice bottom sheet on iOS.
@interface WalletReminderNoticeCoordinator : ChromeCoordinator

// Initializes the coordinator with base view controller, title and legal
// message lines.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                         legalMessageLines:
                             (autofill::LegalMessageLines)legalMessageLines
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_COORDINATOR_WALLET_REMINDER_NOTICE_COORDINATOR_H_
