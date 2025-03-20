// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_util.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/ASPasskeyCredentialIdentity+credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential_provider_creation_notifier.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"

using base::SysNSStringToUTF8;

namespace {

// Appends "data" at the end of "container".
void Append(std::vector<uint8_t>& container, NSData* data) {
  base::span<const uint8_t> span = base::apple::NSDataToSpan(data);
  // Use append_range when C++23 is available.
  container.insert(container.end(), span.begin(), span.end());
}

// Creates an ExtensionInputData structure from the prf inputs provided in the
// passkey request.
webauthn::passkey_model_utils::ExtensionInputData
ExtensionInputDataFromPRFInputs(NSArray<NSData*>* prf_inputs) {
  if (prf_inputs) {
    return webauthn::passkey_model_utils::ExtensionInputData(
        ([prf_inputs count] > 0) ? base::apple::NSDataToSpan(prf_inputs[0])
                                 : std::vector<uint8_t>(),
        ([prf_inputs count] > 1) ? base::apple::NSDataToSpan(prf_inputs[1])
                                 : std::vector<uint8_t>());
  }
  return webauthn::passkey_model_utils::ExtensionInputData();
}

// Reads the PRF's result into 1 or 2 PRF outputs if the passkey request had
// provided PRF input data. Returns nil otherwise.
NSMutableArray<NSData*>* PRFOutputsFromExtensionOutputData(
    const webauthn::passkey_model_utils::ExtensionOutputData&
        extension_output_data) {
  static constexpr size_t kPRFOutputSize = 32u;

  size_t result_size = extension_output_data.prf_result.size();
  bool hasOneOutput = result_size == kPRFOutputSize;
  bool hasTwoOutputs = result_size == 2u * kPRFOutputSize;

  // The PRF result can be empty, have exactly 1 output or exactly 2 outputs.
  CHECK(result_size == 0u || hasOneOutput || hasTwoOutputs)
      << "Invalid PRF result size: " << result_size;

  if (hasOneOutput || hasTwoOutputs) {
    NSMutableArray<NSData*>* prf_outputs = [NSMutableArray array];
    [prf_outputs
        addObject:[[NSData alloc]
                      initWithBytes:extension_output_data.prf_result.data()
                             length:kPRFOutputSize]];
    if (hasTwoOutputs) {
      [prf_outputs
          addObject:[[NSData alloc]
                        initWithBytes:extension_output_data.prf_result.data() +
                                      kPRFOutputSize
                               length:kPRFOutputSize]];
    }
    return prf_outputs;
  }
  return nil;
}

// Wrapper around passkey_model_utils's MakeAuthenticatorDataForAssertion
// function.
NSData* MakeAuthenticatorDataForAssertion(
    NSString* rp_id,
    const webauthn::passkey_model_utils::ExtensionInputData&
        extension_input_data) {
  std::vector<uint8_t> authenticator_data =
      webauthn::passkey_model_utils::MakeAuthenticatorDataForAssertion(
          SysNSStringToUTF8(rp_id), extension_input_data);
  return [NSData dataWithBytes:authenticator_data.data()
                        length:authenticator_data.size()];
}

// Generates the signature during the passkey assertion process by decrypting
// the passkey using the security domain secret and then using the decrypted
// passkey to call passkey_model_utils's GenerateEcSignature function.
NSData* GenerateSignature(NSData* authenticator_data,
                          NSData* client_data_hash,
                          const std::string& private_key) {
  // Prepare the signed data.
  std::vector<uint8_t> signed_over_data;
  Append(signed_over_data, authenticator_data);
  Append(signed_over_data, client_data_hash);

  // Compute signature.
  std::optional<std::vector<uint8_t>> signature =
      webauthn::passkey_model_utils::GenerateEcSignature(
          base::as_byte_span(private_key), signed_over_data);
  if (!signature) {
    return nil;
  }

  return [NSData dataWithBytes:signature->data() length:signature->size()];
}

void SaveToIdentityStore(id<Credential> credential, ProceduralBlock completion)
    API_AVAILABLE(ios(17.0)) {
  auto stateCompletion = ^(ASCredentialIdentityStoreState* state) {
    if (state.enabled) {
      // Update ASCredentialIdentityStore to make the passkey immediately
      // available locally.
      NSMutableArray<id<ASCredentialIdentity>>* storeIdentities =
          [NSMutableArray arrayWithCapacity:1];
      [storeIdentities addObject:[[ASPasskeyCredentialIdentity alloc]
                                     cr_initWithCredential:credential]];
      [ASCredentialIdentityStore.sharedStore
          replaceCredentialIdentityEntries:storeIdentities
                                completion:^(BOOL success, NSError* error) {
                                  completion();
                                }];
    } else {
      completion();
    }
  };
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:stateCompletion];
}

// Saves a newly created passkey to the user defaults credential store. This
// credential store will be read by Chrome if it is currently running, or the
// next time it runs, to sync the newly created passkeys in the user's account.
void SaveCredential(id<Credential> credential) {
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
      initWithUserDefaults:app_group::GetGroupUserDefaults()
                       key:key];

  if ([store credentialWithRecordIdentifier:credential.recordIdentifier]) {
    [store updateCredential:credential];
  } else {
    [store addCredential:credential];
  }

  [store saveDataWithCompletion:^(NSError* error) {
    if (error != nil) {
      return;
    }

    if (@available(iOS 17.0, *)) {
      SaveToIdentityStore(credential, ^{
        // Notify Chrome that a new passkey was created
        [CredentialProviderCreationNotifier notifyCredentialCreated];
      });
    }
  }];
}

// Returns the UserVerificationPreference based on the provided
// `user_verification_preference_string`. The passed string is expected to match
// one of the user verification preference options made available by the
// WebAuthn API.
UserVerificationPreference UserVerificationPreferenceFromString(
    ASAuthorizationPublicKeyCredentialUserVerificationPreference
        user_verification_preference_string) {
  if ([user_verification_preference_string
          isEqualToString:
              ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired]) {
    return UserVerificationPreference::kRequired;
  } else if (
      [user_verification_preference_string
          isEqualToString:
              ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred]) {
    return UserVerificationPreference::kPreferred;
  } else if (
      [user_verification_preference_string
          isEqualToString:
              ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged]) {
    return UserVerificationPreference::kDiscouraged;
  } else {
    // Either indicates that the WebAuthn API changed, or that the website
    // provided an unexpected/empty string.
    return UserVerificationPreference::kOther;
  }
}

}  // namespace

std::optional<sync_pb::WebauthnCredentialSpecifics_Encrypted>
DecryptCredentialSecrets(id<Credential> credential,
                         NSArray<NSData*>* security_domain_secrets) {
  if ([security_domain_secrets count] == 0) {
    return std::nullopt;
  }

  // Decrypt the private key using the security domain secret.
  sync_pb::WebauthnCredentialSpecifics credential_specifics;
  if ([credential.privateKey length] > 0) {
    credential_specifics.set_private_key(credential.privateKey.bytes,
                                         credential.privateKey.length);
  } else if ([credential.encrypted length] > 0) {
    credential_specifics.set_encrypted(credential.encrypted.bytes,
                                       credential.encrypted.length);
  } else {
    return std::nullopt;
  }

  for (NSData* security_domain_secret in security_domain_secrets) {
    std::vector<uint8_t> trusted_vault_key;
    Append(trusted_vault_key, security_domain_secret);

    sync_pb::WebauthnCredentialSpecifics_Encrypted credential_secrets;
    if (webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
            trusted_vault_key, credential_specifics, &credential_secrets)) {
      return credential_secrets;
    }
  }

  return std::nullopt;
}

PasskeyCreationOutput PerformPasskeyCreation(
    NSData* client_data_hash,
    NSString* rp_id,
    NSString* user_name,
    NSData* user_handle,
    NSString* gaia,
    NSArray<NSData*>* security_domain_secrets,
    NSArray<NSData*>* prf_inputs) API_AVAILABLE(ios(17.0)) {
  if ([security_domain_secrets count] == 0) {
    return {};
  }

  std::vector<uint8_t> trusted_vault_key;
  Append(trusted_vault_key, security_domain_secrets[0]);

  // Convert input arguments to std equivalents for use in functions below.
  std::vector<uint8_t> user_id;
  Append(user_id, user_handle);
  std::string rp_id_str = SysNSStringToUTF8(rp_id);
  std::string user_name_str = SysNSStringToUTF8(user_name);

  webauthn::passkey_model_utils::ExtensionInputData extension_input_data =
      ExtensionInputDataFromPRFInputs(prf_inputs);
  webauthn::passkey_model_utils::ExtensionOutputData extension_output_data;

  // Generate a key pair containing the webauthn specifics and the public key.
  std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
      generated_passkey =
          webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
              rp_id_str,
              webauthn::PasskeyModel::UserEntity(user_id, user_name_str,
                                                 user_name_str),
              trusted_vault_key, /*trusted_vault_key_version=*/0,
              extension_input_data, &extension_output_data);
  sync_pb::WebauthnCredentialSpecifics passkey = generated_passkey.first;
  std::vector<uint8_t> public_key_spki_der = generated_passkey.second;

  base::span<const uint8_t> cred_id =
      base::as_byte_span(passkey.credential_id());
  NSData* credential_id = [NSData dataWithBytes:cred_id.data()
                                         length:cred_id.size()];
  std::vector<uint8_t> attestation_object_for_creation =
      webauthn::passkey_model_utils::MakeAttestationObjectForCreation(
          rp_id_str, cred_id, public_key_spki_der, extension_input_data);
  NSData* attestation_object =
      [NSData dataWithBytes:attestation_object_for_creation.data()
                     length:attestation_object_for_creation.size()];

  SaveCredential([[ArchivableCredential alloc] initWithFavicon:nil
                                                          gaia:gaia
                                                       passkey:passkey]);

  return {[ASPasskeyRegistrationCredential
              credentialWithRelyingParty:rp_id
                          clientDataHash:client_data_hash
                            credentialID:credential_id
                       attestationObject:attestation_object],
          PRFOutputsFromExtensionOutputData(extension_output_data)};
}

PasskeyAssertionOutput PerformPasskeyAssertion(
    id<Credential> credential,
    NSData* client_data_hash,
    NSArray<NSData*>* allowed_credentials,
    NSArray<NSData*>* security_domain_secrets,
    NSArray<NSData*>* prf_inputs) API_AVAILABLE(ios(17.0)) {
  if ([security_domain_secrets count] == 0) {
    return {};
  }

  // If the array is empty, then the relying party accepts any passkey
  // credential.
  if (allowed_credentials.count > 0 &&
      ![allowed_credentials containsObject:credential.credentialId]) {
    return {};
  }

  std::optional<sync_pb::WebauthnCredentialSpecifics_Encrypted>
      credential_secrets =
          DecryptCredentialSecrets(credential, security_domain_secrets);
  if (!credential_secrets) {
    return {};
  }

  webauthn::passkey_model_utils::ExtensionInputData extension_input_data =
      ExtensionInputDataFromPRFInputs(prf_inputs);
  NSData* authenticatorData =
      MakeAuthenticatorDataForAssertion(credential.rpId, extension_input_data);
  NSData* signature = GenerateSignature(authenticatorData, client_data_hash,
                                        credential_secrets->private_key());

  if (!signature) {
    return {};
  }

  // Update the credential's last used time.
  credential.lastUsedTime =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  SaveCredential(credential);

  return {[ASPasskeyAssertionCredential
              credentialWithUserHandle:credential.userId
                          relyingParty:credential.rpId
                             signature:signature
                        clientDataHash:client_data_hash
                     authenticatorData:authenticatorData
                          credentialID:credential.credentialId],
          PRFOutputsFromExtensionOutputData(
              extension_input_data.ToOutputData(*credential_secrets))};
}

BOOL ShouldPerformUserVerificationForPreference(
    ASAuthorizationPublicKeyCredentialUserVerificationPreference
        user_verification_preference_string,
    BOOL is_biometric_authentication_enabled) {
  UserVerificationPreference user_verification_preference =
      UserVerificationPreferenceFromString(user_verification_preference_string);

  switch (user_verification_preference) {
    case UserVerificationPreference::kRequired:
      return YES;
    case UserVerificationPreference::kPreferred:
    case UserVerificationPreference::kOther:  // Fall back to the default
                                              // preference as per the WebAuthn
                                              // spec.
      return is_biometric_authentication_enabled;
    case UserVerificationPreference::kDiscouraged:
      return NO;
  }
}
