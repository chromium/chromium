// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import <optional>
#import <string>

namespace sync_pb {
class WebauthnCredentialSpecifics_Encrypted;
}  // namespace sync_pb

@protocol Credential;

// Enum which represents possible user verification preferences.
enum class UserVerificationPreference {
  kRequired = 0,
  kPreferred,
  kDiscouraged,
  kOther,
};

// Decrypts the credential's secrets, like the private key and the hmac secret.
// Can be used to verify if any of the security_domain_secrets from the provided
// array is valid. If the decryption is successful, the results will be stored
// in the provided `credential_secrets` structure.
std::optional<sync_pb::WebauthnCredentialSpecifics_Encrypted>
DecryptCredentialSecrets(id<Credential> credential,
                         NSArray<NSData*>* security_domain_secrets);

// Credential and extension data returned by the passkey creation process.
struct API_AVAILABLE(ios(17.0)) PasskeyCreationOutput {
  ASPasskeyRegistrationCredential* credential;
  NSMutableArray<NSData*>* prf_outputs;
};

// On a success, returns a newly created passkey and extension output data.
// Also, on a success, PasskeyCreationOutput's `prf_outputs` is written to if
// `prf_inputs` is provided. Otherwise, returns a structure with nil members.
//
// `prf_inputs` is provided is PRF support is requested, otherwise, it should be
// nil.
PasskeyCreationOutput PerformPasskeyCreation(
    NSData* client_data_hash,
    NSString* rp_id,
    NSString* user_name,
    NSData* user_handle,
    NSString* gaia,
    NSArray<NSData*>* security_domain_secrets,
    NSArray<NSData*>* prf_inputs) API_AVAILABLE(ios(17.0));

// Credential and extension data returned by the passkey assertion process.
struct API_AVAILABLE(ios(17.0)) PasskeyAssertionOutput {
  ASPasskeyAssertionCredential* credential;
  NSMutableArray<NSData*>* prf_outputs;
};

// On a success, returns a valid passkey assertion structure and extension
// output data. On a success, PasskeyAssertionOutput's `prf_outputs` is written
// to if `prf_inputs` is provided. Otherwise, returns a structure with nil
// members.
//
// `prf_inputs` is provided is PRF support is requested, otherwise, it should be
// nil.
PasskeyAssertionOutput PerformPasskeyAssertion(
    id<Credential> credential,
    NSData* client_data_hash,
    NSArray<NSData*>* allowed_credentials,
    NSArray<NSData*>* security_domain_secrets,
    NSArray<NSData*>* prf_inputs) API_AVAILABLE(ios(17.0));

// Returns whether or not the user should be asked to re-authenticate depending
// on the provided `user_verification_preference_string` and whether biometric
// authentication is enabled for the device.
BOOL ShouldPerformUserVerificationForPreference(
    ASAuthorizationPublicKeyCredentialUserVerificationPreference
        user_verification_preference_string,
    BOOL is_biometric_authentication_enabled);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_
