// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_UTIL_H_

#import <Foundation/Foundation.h>

// Queries Keychain Services for the passed identifier password.
NSString* PasswordWithKeychainIdentifier(NSString* identifier);

// Stores `password` in Keychain Services using `identifier` as its identifier
// for later query. Returns `YES` if saving was successful and `NO` otherwise.
BOOL StorePasswordInKeychain(NSString* password, NSString* identifier);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_UTIL_H_
