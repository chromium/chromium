// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides methods to encode credential metadata for storage in the
// macOS keychain.

#ifndef DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
#define DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "crypto/aead.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"

namespace device {

class PublicKeyCredentialUserEntity;

namespace fido {
namespace mac {

// CredentialMetadata is the metadata for a Touch ID credential stored, in an
// encrypted/authenticated format, in the macOS keychain.  Values of this type
// should be moved whenever possible.
struct COMPONENT_EXPORT(DEVICE_FIDO) CredentialMetadata {
 public:
  static CredentialMetadata FromPublicKeyCredentialUserEntity(
      const PublicKeyCredentialUserEntity&,
      bool is_resident);

  CredentialMetadata(std::vector<uint8_t> user_id_,
                     std::string user_name_,
                     std::string user_display_name_,
                     bool is_resident_);
  CredentialMetadata(const CredentialMetadata&);
  CredentialMetadata(CredentialMetadata&&);
  CredentialMetadata& operator=(CredentialMetadata&&);
  ~CredentialMetadata();

  PublicKeyCredentialUserEntity ToPublicKeyCredentialUserEntity();

  // The following correspond to the fields of the same name in
  // PublicKeyCredentialUserEntity
  // (https://www.w3.org/TR/webauthn/#sctn-user-credential-params).
  std::vector<uint8_t> user_id;
  std::string user_name;
  std::string user_display_name;

  // Whether this credential has the resident key (rk) bit and may be returned
  // in response to GetAssertion requests with an empty allowList.
  bool is_resident;
};

// Generates a random secret for encrypting and authenticating credential
// metadata for storage in the macOS keychain.
//
// Chrome stores this secret in the Profile Prefs. This allows us to logically
// separate credentials per Chrome profile despite being stored in the same
// keychain. It also guarantees that account metadata in the OS keychain is
// rendered unusable after the Chrome profile and the associated encryption key
// have been deleted, in order to limit leakage of credential metadata into the
// keychain.
COMPONENT_EXPORT(DEVICE_FIDO)
std::string GenerateCredentialMetadataSecret();

// SealCredentialId encrypts the given CredentialMetadata into a credential id.
//
// Credential IDs have following format:
//
//    | version  |    nonce   | AEAD(pt=CBOR(metadata), |
//    | (1 byte) | (12 bytes) |      nonce=nonce,          |
//    |          |            |      ad=(version, rpID))   |
//
// with version as 0x00, a random 12-byte nonce, and using AES-256-GCM as the
// AEAD.
//
// The |user_name| and |user_display_name| fields may be truncated before
// encryption. The truncated values are guaranteed to be valid UTF-8.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> SealCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    const CredentialMetadata& metadata);

// UnsealCredentialId attempts to decrypt a CredentialMetadata from a credential
// id.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<CredentialMetadata> UnsealCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id);

// EncodeRpIdAndUserId encodes the concatenation of RP ID and user ID for
// storage in the macOS keychain.
//
// This encoding allows lookup of credentials for a given RP and user but
// without the credential ID.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::string> EncodeRpIdAndUserId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> user_id);

// EncodeRpId encodes the given RP ID for storage in the macOS keychain.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::string> EncodeRpId(const std::string& secret,
                                       const std::string& rp_id);

// DecodeRpId attempts to decode a given RP ID from the keychain.
//
// This can be used to test whether a set of credential metadata was created
// under the given secret without knowing the RP ID (which would be required to
// unseal a credential ID).
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::string> DecodeRpId(const std::string& secret,
                                       const std::string& ciphertext);

// Seals a legacy V0 credential ID.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> SealLegacyV0CredentialIdForTestingOnly(
    const std::string& secret,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    const std::string& user_name,
    const std::string& user_display_name);

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
