// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_DETAILS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_DETAILS_CONSUMER_H_

#import <Foundation/Foundation.h>

// Sets the Add Password details for consumer.
@protocol AddPasswordDetailsConsumer <NSObject>

// Sets the account where passwords are being saved to, or nil if passwords are
// only being saved locally.
- (void)setAccountSavingPasswords:(NSString*)email;

// Called when the validation to find duplicate existing credentials has been
// completed.
- (void)onDuplicateCheckCompletion:(BOOL)duplicateFound;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_ADD_PASSWORD_DETAILS_CONSUMER_H_
