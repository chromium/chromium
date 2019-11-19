// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

@interface PasswordManagerAppInterface : NSObject

// Sets preferences required for autosign-in to true.
+ (void)setAutosigninPreferences;

// Stores a credential to the password store.
+ (NSError*)storeCredentialWithUsername:(NSString*)username
                               password:(NSString*)password;

// Clears any credentials that were stored during a test run.
+ (void)clearCredentials;

// Executes the javascript to fetch credentials in a background tab. There must
// be two tabs open before calling this method.
+ (void)getCredentialsInTabAtIndex:(int)index;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_MANAGER_APP_INTERFACE_H_
