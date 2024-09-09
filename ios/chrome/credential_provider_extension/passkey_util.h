// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "ios/chrome/credential_provider_extension/passkey_keychain_provider.h"

@protocol Credential;

typedef void (^FetchKeyCompletionBlock)(
    const PasskeyKeychainProvider::SharedKeyList& keyList);

// Fetches the Security Domain Secret and calls the completion block
// with the Security Domain Secret as the input argument.
void FetchSecurityDomainSecret(
    NSString* gaia,
    UINavigationController* navigation_controller,
    PasskeyKeychainProvider::ReauthenticatePurpose purpose,
    FetchKeyCompletionBlock callback);

// On a success, returns a newly created passkey.
// Returns nil otherwise.
ASPasskeyRegistrationCredential* PerformPasskeyCreation(
    NSData* client_data_hash,
    NSString* rp_id,
    NSString* user_name,
    NSData* user_handle,
    const PasskeyKeychainProvider::SharedKeyList& keyList)
    API_AVAILABLE(ios(17.0));

// On a success, returns a valid passkey assertion structure.
// Returns nil otherwise.
ASPasskeyAssertionCredential* PerformPasskeyAssertion(
    id<Credential> credential,
    NSData* client_data_hash,
    NSArray<NSData*>* allowed_credentials,
    const PasskeyKeychainProvider::SharedKeyList& keyList)
    API_AVAILABLE(ios(17.0));

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_
