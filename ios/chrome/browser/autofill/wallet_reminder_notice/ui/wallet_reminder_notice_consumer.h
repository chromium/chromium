// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_CONSUMER_H_

#import <Foundation/Foundation.h>

@class SaveCardMessageWithLinks;

// Consumer interface for updating the Wallet Reminder Notice bottom sheet.
@protocol WalletReminderNoticeConsumer <NSObject>

// Sets the title displayed on the bottom sheet.
- (void)setTitleString:(NSString*)titleString;

// Sets the hyperlinked disclaimer text lines.
- (void)setDisclaimerText:(NSArray<SaveCardMessageWithLinks*>*)disclaimerText;

// Sets the text for the primary action button.
- (void)setPrimaryActionString:(NSString*)primaryActionString;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_WALLET_REMINDER_NOTICE_UI_WALLET_REMINDER_NOTICE_CONSUMER_H_
