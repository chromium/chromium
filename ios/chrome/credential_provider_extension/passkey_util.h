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

// Decrypts the credential's secrets, like the private key and the hmac secret.
// Can be used to verify if any of the security_domain_secrets from the provided
// array is valid. If the decryption is successful, the results will be stored
// in the provided `credential_secrets` structure.
std::optional<sync_pb::WebauthnCredentialSpecifics_Encrypted>
DecryptCredentialSecrets(id<Credential> credential,
                         NSArray<NSData*>* security_domain_secrets);

// Credential and extension data returned by the passkey creation process.
struct PasskeyCreationOutput {
  ASPasskeyRegistrationCredential* credential;
  NSMutableArray<NSData*>* prf_outputs;
};

// On a success, returns a newly created passkey and extension output data.
// Also, on a success, PasskeyCreationOutput's `prf_outputs` is written to if
// `prf_inputs` is provided. Otherwise, returns a structure with nil members.
//
// `prf_inputs` is provided is PRF support is requested, otherwise, it should be
// nil. `did_complete_uv` should be true iff user verification was completed for
// this operation.
PasskeyCreationOutput PerformPasskeyCreation(
    NSData* client_data_hash,
    NSString* rp_id,
    NSString* user_name,
    NSData* user_handle,
    NSString* gaia,
    NSArray<NSData*>* security_domain_secrets,
    NSArray<NSData*>* prf_inputs,
    bool did_complete_uv);

// Credential and extension data returned by the passkey assertion process.
struct PasskeyAssertionOutput {
  ASPasskeyAssertionCredential* credential;
  NSMutableArray<NSData*>* prf_outputs;
};

// On a success, returns a valid passkey assertion structure and extension
// output data. On a success, PasskeyAssertionOutput's `prf_outputs` is written
// to if `prf_inputs` is provided. Otherwise, returns a structure with nil
// members.
//
// `prf_inputs` is provided if PRF support is requested, otherwise, it should be
// nil. `did_complete_uv` should be true iff user verification was completed for
// this operation.
PasskeyAssertionOutput PerformPasskeyAssertion(
    id<Credential> credential,
    NSData* client_data_hash,
    NSArray<NSData*>* allowed_credentials,
    NSArray<NSData*>* security_domain_secrets,
    NSArray<NSData*>* prf_inputs,
    bool did_complete_uv);

// Returns whether or not the user should be asked to re-authenticate depending
// on the provided `user_verification_preference_string` and whether biometric
// authentication is enabled for the device. If the request is a conditional
// create, then the user verification should not be performed.
BOOL ShouldPerformUserVerificationForPreference(
    ASAuthorizationPublicKeyCredentialUserVerificationPreference
        user_verification_preference_string,
    BOOL is_biometric_authentication_enabled,
    BOOL is_conditional_create);

// Saves a passkey credential to the user defaults credential store. This
// credential store will be read by Chrome if it is currently running, or the
// next time it runs, to sync the newly created passkeys in the user's account.
//
// Additionally, updates ASCredentialIdentityStore so that the passkey is
// correctly surfaced or hidden from the sign-in sheet.
void SavePasskeyCredential(id<Credential> credential);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_UTIL_H_
