// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_CONSUMER_H_

#import <Foundation/Foundation.h>

@class AccountErrorUIInfo;

@protocol AccountMenuConsumer <NSObject>

// Updates error section.
- (void)updateErrorSection:(AccountErrorUIInfo*)error;

// Updates the list of accounts.
- (void)updateAccountListWithGaiaIDsToAdd:(NSArray<NSString*>*)indicesToAdd
                          gaiaIDsToRemove:(NSArray<NSString*>*)gaiaIDsToRemove
                            gaiaIDsToKeep:(NSArray<NSString*>*)gaiaIDsToKeep;

// Updates the primary account details.
- (void)updatePrimaryAccount;

// Called wthen the account switch starts.
- (void)switchingStarted;

// Tells the consumer the switch is no longer in progress.
- (void)switchingStopped;

// Enable or disable the UI interaction.
- (void)setUserInteractionsEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_CONSUMER_H_
