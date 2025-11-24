// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_util.h"

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/gpm_user_verification_policy.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "device/fido/fido_types.h"
#import "device/fido/fido_user_verification_requirement.h"
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

  auto span = base::span(extension_output_data.prf_result);

  // The PRF result can be empty, have exactly 1 output or exactly 2 outputs.
  CHECK_EQ(span.size() % kPRFOutputSize, 0u)
      << "Invalid PRF result size: " << span.size();
  CHECK_LE(span.size() / kPRFOutputSize, 2u)
      << "Invalid PRF result size: " << span.size();

  if (span.empty()) {
    return nil;
  }

  NSMutableArray<NSData*>* result = [NSMutableArray array];
  while (span.size() >= kPRFOutputSize) {
    auto [head, rest] = span.split_at<kPRFOutputSize>();

    [result addObject:[[NSData alloc] initWithBytes:head.data()
                                             length:head.size()]];

    span = rest;
  }

  return result;
}

// Wrapper around passkey_model_utils's MakeAuthenticatorDataForAssertion
// function.
NSData* MakeAuthenticatorDataForAssertion(NSString* rp_id,
                                          bool did_complete_uv) {
  std::vector<uint8_t> authenticator_data =
      webauthn::passkey_model_utils::MakeAuthenticatorDataForAssertion(
          SysNSStringToUTF8(rp_id), did_complete_uv);
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

void SaveToIdentityStore(id<Credential> credential,
                         ProceduralBlock completion) {
  auto stateCompletion = ^(ASCredentialIdentityStoreState* state) {
    if (!state.enabled) {
      completion();
      return;
    }

    NSArray<id<ASCredentialIdentity>>* storeIdentities =
        @[ [[ASPasskeyCredentialIdentity alloc]
            cr_initWithCredential:credential] ];
    void (^storeCompletion)(BOOL, NSError*) = ^(BOOL success, NSError* error) {
      completion();
    };

    if (credential.hidden) {
      [ASCredentialIdentityStore.sharedStore
          removeCredentialIdentityEntries:storeIdentities
                               completion:storeCompletion];
    } else {
      [ASCredentialIdentityStore.sharedStore
          saveCredentialIdentityEntries:storeIdentities
                             completion:storeCompletion];
    }
  };
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:stateCompletion];
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
    NSArray<NSData*>* prf_inputs,
    bool did_complete_uv) {
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
  auto [passkey, public_key_spki_der] =
      webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
          rp_id_str,
          webauthn::PasskeyModel::UserEntity(user_id, user_name_str,
                                             user_name_str),
          trusted_vault_key, /*trusted_vault_key_version=*/0,
          extension_input_data, &extension_output_data);

  base::span<const uint8_t> cred_id =
      base::as_byte_span(passkey.credential_id());
  webauthn::passkey_model_utils::SerializedAttestationObject
      serialized_attestation_object =
          webauthn::passkey_model_utils::MakeAttestationObjectForCreation(
              rp_id_str, did_complete_uv, cred_id, public_key_spki_der);

  SavePasskeyCredential([[ArchivableCredential alloc] initWithFavicon:nil
                                                                 gaia:gaia
                                                              passkey:passkey]);

  NSData* credential_id = [NSData dataWithBytes:cred_id.data()
                                         length:cred_id.size()];
  NSData* attestation_object = [NSData
      dataWithBytes:serialized_attestation_object.attestation_object.data()
             length:serialized_attestation_object.attestation_object.size()];
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
    NSArray<NSData*>* prf_inputs,
    bool did_complete_uv) {
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
      MakeAuthenticatorDataForAssertion(credential.rpId, did_complete_uv);
  NSData* signature = GenerateSignature(authenticatorData, client_data_hash,
                                        credential_secrets->private_key());

  if (!signature) {
    return {};
  }

  // Update the credential's last used time.
  credential.lastUsedTime =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  SavePasskeyCredential(credential);

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
    BOOL is_biometric_authentication_enabled,
    BOOL is_conditional_create) {
  if (is_conditional_create) {
    return NO;
  }

  // Fall back to the `kPreferred` UV requirement as per the WebAuthn spec.
  std::string user_verification_requirement_string =
      SysNSStringToUTF8(user_verification_preference_string);
  return webauthn::GpmWillDoUserVerification(
      device::ConvertToUserVerificationRequirement(
          user_verification_requirement_string)
          .value_or(device::UserVerificationRequirement::kPreferred),
      is_biometric_authentication_enabled);
}

void SavePasskeyCredential(id<Credential> credential) {
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

    SaveToIdentityStore(credential, ^{
      // TODO(crbug.com/432260316): Consider renaming this class as its purpose
      // is to trigger migration, but not necessarily for creations only.
      [CredentialProviderCreationNotifier notifyCredentialCreated];
    });
  }];
}
