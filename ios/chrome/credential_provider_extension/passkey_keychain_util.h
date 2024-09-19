// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_UTIL_H_

#import "base/ios/block_types.h"
#import "ios/chrome/credential_provider_extension/passkey_keychain_provider.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"

// Fetches the Security Domain Secret and calls the completion block
// with the Security Domain Secret as the input argument.
void FetchSecurityDomainSecret(
    NSString* gaia,
    UINavigationController* navigation_controller,
    PasskeyKeychainProvider::ReauthenticatePurpose purpose,
    FetchKeyCompletionBlock completion);

// Marks the security domain secret vault keys as stale and calls the completion
// block.
void MarkKeysAsStale(NSString* gaia, ProceduralBlock completion);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_UTIL_H_
