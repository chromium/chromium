// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_UTIL_H_
#define IOS_COMPONENTS_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_UTIL_H_

#import <Foundation/Foundation.h>

namespace credential_provider_extension {

// Queries Keychain Services for the passed identifier password.
// Returns nil if `identifier` is nil or if no password found for `identifier`.
NSString* PasswordWithKeychainIdentifier(NSString* identifier);

// Stores `password` in Keychain Services using `identifier` as its identifier
// for later query. Returns `YES` if saving was successful and `NO` otherwise.
BOOL StorePasswordInKeychain(NSString* password, NSString* identifier);

}  // namespace credential_provider_extension

#endif  // IOS_COMPONENTS_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_UTIL_H_
