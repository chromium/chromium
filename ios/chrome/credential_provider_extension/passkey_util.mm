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
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
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

// Wrapper around passkey_model_utils's MakeAuthenticatorDataForAssertion
// function.
NSData* MakeAuthenticatorDataForAssertion(NSString* rp_id) {
  std::vector<uint8_t> authenticator_data =
      webauthn::passkey_model_utils::MakeAuthenticatorDataForAssertion(
          SysNSStringToUTF8(rp_id));
  return [NSData dataWithBytes:authenticator_data.data()
                        length:authenticator_data.size()];
}

// Generates the signature during the passkey assertion process by decrypting
// the passkey using the security domain secret and then using the decrypted
// passkey to call passkey_model_utils's GenerateEcSignature function.
NSData* GenerateSignature(NSData* encrypted_private_key,
                          NSData* encrypted_message,
                          NSData* authenticator_data,
                          NSData* client_data_hash,
                          NSData* security_domain_secret) {
  if (!security_domain_secret) {
    return nil;
  }

  std::vector<uint8_t> trusted_vault_key;
  Append(trusted_vault_key, security_domain_secret);

  // Decrypt the private key using the security domain secret.
  sync_pb::WebauthnCredentialSpecifics credential_specifics;
  if (encrypted_private_key) {
    credential_specifics.set_private_key(encrypted_private_key.bytes,
                                         encrypted_private_key.length);
  } else if (encrypted_message) {
    credential_specifics.set_encrypted(encrypted_message.bytes,
                                       encrypted_message.length);
  } else {
    return nil;
  }

  sync_pb::WebauthnCredentialSpecifics_Encrypted credential_secrets;
  if (!webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
          trusted_vault_key, credential_specifics, &credential_secrets)) {
    // TODO(crbug.com/355047427): On the first failed attempt, mark keys as
    // stale, re-fetch the keys and try to decrypt again.
    return nil;
  }

  // Prepare the signed data.
  std::vector<uint8_t> signed_over_data;
  Append(signed_over_data, authenticator_data);
  Append(signed_over_data, client_data_hash);

  // Compute signature.
  std::optional<std::vector<uint8_t>> signature =
      webauthn::passkey_model_utils::GenerateEcSignature(
          base::as_byte_span(credential_secrets.private_key()),
          signed_over_data);
  if (!signature) {
    return nil;
  }

  return [NSData dataWithBytes:signature->data() length:signature->size()];
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

    [CredentialProviderCreationNotifier notifyCredentialCreated];
  }];
}

}  // namespace

ASPasskeyRegistrationCredential* PerformPasskeyCreation(
    NSData* client_data_hash,
    NSString* rp_id,
    NSString* user_name,
    NSData* user_handle,
    NSString* gaia,
    NSData* security_domain_secret) API_AVAILABLE(ios(17.0)) {
  if (!security_domain_secret) {
    return nil;
  }

  std::vector<uint8_t> trusted_vault_key;
  Append(trusted_vault_key, security_domain_secret);

  // Convert input arguments to std equivalents for use in functions below.
  std::vector<uint8_t> user_id;
  Append(user_id, user_handle);
  std::string rp_id_str = SysNSStringToUTF8(rp_id);
  std::string user_name_str = SysNSStringToUTF8(user_name);

  // Generate a key pair containing the webauthn specifics and the public key.
  std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
      generated_passkey =
          webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
              rp_id_str,
              webauthn::PasskeyModel::UserEntity(user_id, user_name_str,
                                                 user_name_str),
              trusted_vault_key,
              /*trusted_vault_key_version=*/0);
  sync_pb::WebauthnCredentialSpecifics passkey = generated_passkey.first;
  std::vector<uint8_t> public_key_spki_der = generated_passkey.second;

  SaveCredential([[ArchivableCredential alloc] initWithFavicon:nil
                                                          gaia:gaia
                                                       passkey:passkey]);

  base::span<const uint8_t> cred_id =
      base::as_byte_span(passkey.credential_id());
  NSData* credential_id = [NSData dataWithBytes:cred_id.data()
                                         length:cred_id.size()];
  std::vector<uint8_t> authenticator_data =
      webauthn::passkey_model_utils::MakeAuthenticatorDataForCreation(
          rp_id_str, cred_id, public_key_spki_der);
  NSData* attestation_object = [NSData dataWithBytes:authenticator_data.data()
                                              length:authenticator_data.size()];

  return [ASPasskeyRegistrationCredential
      credentialWithRelyingParty:rp_id
                  clientDataHash:client_data_hash
                    credentialID:credential_id
               attestationObject:attestation_object];
}

ASPasskeyAssertionCredential* PerformPasskeyAssertion(
    id<Credential> credential,
    NSData* client_data_hash,
    NSArray<NSData*>* allowed_credentials,
    NSData* security_domain_secret) API_AVAILABLE(ios(17.0)) {
  if (!security_domain_secret) {
    return nil;
  }

  // If the array is empty, then the relying party accepts any passkey
  // credential.
  if (allowed_credentials.count > 0 &&
      ![allowed_credentials containsObject:credential.credentialId]) {
    return nil;
  }

  NSData* authenticatorData =
      MakeAuthenticatorDataForAssertion(credential.rpId);
  NSData* signature = GenerateSignature(
      credential.privateKey, credential.encrypted, authenticatorData,
      client_data_hash, security_domain_secret);

  // Update the credential's last used time.
  credential.lastUsedTime =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  SaveCredential(credential);

  return [ASPasskeyAssertionCredential
      credentialWithUserHandle:credential.userId
                  relyingParty:credential.rpId
                     signature:signature
                clientDataHash:client_data_hash
             authenticatorData:authenticatorData
                  credentialID:credential.credentialId];
}
