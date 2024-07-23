// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_util.h"

#import "base/containers/span.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/common/credential_provider/credential.h"

using base::SysNSStringToUTF8;

namespace {

// Appends "data" at the end of "container".
void Append(std::vector<uint8_t>& container, NSData* data) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.bytes);
  container.insert(container.end(), bytes, bytes + data.length);
}

// Returns the security domain secret by fetching it from the vault.
NSData* GetSecurityDomainSecret() {
  // TODO(crbug.com/330355124): Replace this placeholder function with a real
  // vault access.
  std::vector<uint8_t> sds;
  base::HexStringToBytes(
      "1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF", &sds);
  return [NSData dataWithBytes:sds.data() length:sds.size()];
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
                          NSData* client_data_hash) {
  // Retrieve the security domain secret.
  NSData* security_domain_secret = GetSecurityDomainSecret();
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

}  // namespace

ASPasskeyAssertionCredential* PerformPasskeyAssertion(
    id<Credential> credential,
    NSData* clientDataHash,
    NSArray<NSData*>* allowedCredentials) API_AVAILABLE(ios(17.0)) {
  // If the array is empty, then the relying party accepts any passkey
  // credential.
  if (allowedCredentials.count > 0 &&
      ![allowedCredentials containsObject:credential.credentialId]) {
    return nil;
  }

  NSData* authenticatorData =
      MakeAuthenticatorDataForAssertion(credential.rpId);
  NSData* signature =
      GenerateSignature(credential.privateKey, credential.encrypted,
                        authenticatorData, clientDataHash);

  return [ASPasskeyAssertionCredential
      credentialWithUserHandle:credential.userId
                  relyingParty:credential.rpId
                     signature:signature
                clientDataHash:clientDataHash
             authenticatorData:authenticatorData
                  credentialID:credential.credentialId];
}
