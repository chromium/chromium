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
#include "base/strings/string_piece_forward.h"
#include "crypto/aead.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class PublicKeyCredentialUserEntity;

namespace fido {
namespace mac {

// CredentialMetadata is the metadata for a Touch ID credential stored, in an
// encrypted/authenticated format, in the macOS keychain.
struct COMPONENT_EXPORT(DEVICE_FIDO) CredentialMetadata {
 public:
  enum class Version : uint8_t {
    kV0 = 0,
    kV1 = 1,
    kV2 = 2,
    // Update `kCurrentVersion` when adding a value here.
  };

  // The version supported by `SealCredentialId()`. Older versions can only be
  // unsealed.
  static constexpr Version kCurrentVersion = Version::kV2;

  static CredentialMetadata FromPublicKeyCredentialUserEntity(
      const PublicKeyCredentialUserEntity&,
      bool is_resident);

  CredentialMetadata(Version version,
                     std::vector<uint8_t> user_id,
                     std::string user_name,
                     std::string user_display_name,
                     bool is_resident);
  CredentialMetadata(const CredentialMetadata&);
  CredentialMetadata(CredentialMetadata&&);
  CredentialMetadata& operator=(CredentialMetadata&&);
  ~CredentialMetadata();

  PublicKeyCredentialUserEntity ToPublicKeyCredentialUserEntity() const;

  // The version used when unsealing the credential ID.
  Version version;

  // The following correspond to the fields of the same name in
  // PublicKeyCredentialUserEntity
  // (https://www.w3.org/TR/webauthn/#sctn-user-credential-params).
  //
  // The |user_name| and |user_display_name| fields may be truncated before
  // encryption. The truncated values are guaranteed to be valid UTF-8.
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
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> SealCredentialId(const std::string& secret,
                                      const std::string& rp_id,
                                      const CredentialMetadata& metadata);

// UnsealCredentialId attempts to decrypt a CredentialMetadata from a credential
// id.
COMPONENT_EXPORT(DEVICE_FIDO)
absl::optional<CredentialMetadata> UnsealCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id);

// EncodeRpIdAndUserId encodes the concatenation of RP ID and user ID for
// storage in the macOS keychain.
//
// This encoding allows lookup of credentials for a given RP and user but
// without the credential ID.
COMPONENT_EXPORT(DEVICE_FIDO)
std::string EncodeRpIdAndUserId(const std::string& secret,
                                const std::string& rp_id,
                                base::span<const uint8_t> user_id);

// EncodeRpId encodes the given RP ID for storage in the macOS keychain.
COMPONENT_EXPORT(DEVICE_FIDO)
std::string EncodeRpId(const std::string& secret, const std::string& rp_id);

// DecodeRpId attempts to decode a given RP ID from the keychain.
//
// This can be used to test whether a set of credential metadata was created
// under the given secret without knowing the RP ID (which would be required to
// unseal a credential ID).
COMPONENT_EXPORT(DEVICE_FIDO)
absl::optional<std::string> DecodeRpId(const std::string& secret,
                                       const std::string& ciphertext);

// Seals a legacy V0 or V1 credential ID.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> SealLegacyCredentialIdForTestingOnly(
    CredentialMetadata::Version version,
    const std::string& secret,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    const std::string& user_name,
    const std::string& user_display_name,
    bool is_resident);

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
