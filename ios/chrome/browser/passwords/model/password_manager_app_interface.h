// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

@interface PasswordManagerAppInterface : NSObject

// Stores a credential to the password store. Similar to the other functions in
// this file, but also specifies whether the credential was `shared` (received
// from some other user).
+ (NSError*)storeCredentialWithUsername:(NSString*)username
                               password:(NSString*)password
                                    URL:(NSURL*)URL
                                 shared:(BOOL)shared;

// Stores a credential to the password store.
+ (NSError*)storeCredentialWithUsername:(NSString*)username
                               password:(NSString*)password
                                    URL:(NSURL*)URL;

// Stores a credential to the password store. Associates the current WebState's
// last committed URL with the credential.
+ (NSError*)storeCredentialWithUsername:(NSString*)username
                               password:(NSString*)password;

// Returns true if there is a stored credential matching the `username` and
// `password`.
+ (bool)verifyCredentialStoredWithUsername:(NSString*)username
                                  password:(NSString*)password;

// Clears any credentials that were stored during a test run.
+ (bool)clearCredentials;

// Returns the number of stored credentials.
+ (int)storedCredentialsCount;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_MANAGER_APP_INTERFACE_H_
