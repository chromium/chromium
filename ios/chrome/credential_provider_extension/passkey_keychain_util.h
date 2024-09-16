// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_UTIL_H_

#import "ios/chrome/credential_provider_extension/passkey_keychain_provider.h"

typedef void (^FetchKeyCompletionBlock)(
    const PasskeyKeychainProvider::SharedKeyList& keyList);

// Fetches the Security Domain Secret and calls the completion block
// with the Security Domain Secret as the input argument.
void FetchSecurityDomainSecret(
    NSString* gaia,
    UINavigationController* navigation_controller,
    PasskeyKeychainProvider::ReauthenticatePurpose purpose,
    FetchKeyCompletionBlock callback);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_UTIL_H_
