// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import <string>

@protocol Credential;

// Enum which represents possible user verification preferences.
enum class UserVerificationPreference {
  kRequired = 0,
  kPreferred,
  kDiscouraged,
  kOther,
};

// Decrypts the credential's private key. Can be used to verify if any of the
// security_domain_secrets from the provided array is valid.
std::string DecryptPrivateKey(id<Credential> credential,
                              NSArray<NSData*>* security_domain_secrets);

// On a success, returns a newly created passkey.
// Returns nil otherwise.
ASPasskeyRegistrationCredential* PerformPasskeyCreation(
    NSData* client_data_hash,
    NSString* rp_id,
    NSString* user_name,
    NSData* user_handle,
    NSString* gaia,
    NSArray<NSData*>* security_domain_secrets) API_AVAILABLE(ios(17.0));

// On a success, returns a valid passkey assertion structure.
// Returns nil otherwise.
ASPasskeyAssertionCredential* PerformPasskeyAssertion(
    id<Credential> credential,
    NSData* client_data_hash,
    NSArray<NSData*>* allowed_credentials,
    NSArray<NSData*>* security_domain_secrets) API_AVAILABLE(ios(17.0));

// Returns whether or not the user should be asked to re-authenticate depending
// on the provided `userVerificationPreferenceString` and whether biometric
// authentication is enabled for the device.
BOOL ShouldPerformUserVerificationForPreference(
    NSString* user_verification_preference_string,
    BOOL is_biometric_authentication_enabled);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_
