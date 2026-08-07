// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/wallet_reminder_notice/ui/wallet_reminder_notice_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@class WalletReminderNoticeViewController;

// Delegate protocol for handling user interaction events in the view
// controller.
@protocol WalletReminderNoticeViewControllerDelegate <NSObject>

// Notifies the delegate that the primary action button was tapped.
- (void)walletReminderNoticeViewControllerDidTapPrimaryAction:
    (WalletReminderNoticeViewController*)viewController;

// Notifies the delegate that a link in the legal disclaimer was tapped.
- (void)walletReminderNoticeViewController:
            (WalletReminderNoticeViewController*)viewController
                             didTapLinkURL:(NSURL*)URL;

@end

// View controller for the Wallet Reminder Notice bottom sheet.
@interface WalletReminderNoticeViewController
    : ConfirmationAlertViewController <WalletReminderNoticeConsumer>

// Delegate for user interaction events.
@property(nonatomic, weak) id<WalletReminderNoticeViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_VIEW_CONTROLLER_H_
