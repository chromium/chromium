// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides methods to encode credential metadata for storage in the
// macOS keychain.

#ifndef DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
#define DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "crypto/aead.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "device/fido/features.h"

namespace device {

class PublicKeyCredentialUserEntity;

namespace fido::mac {

// CredentialMetadata is the metadata for a Touch ID credential.
//
// This struct is CBOR serialized and stored encrypted in the keychain item for
// the credential. Originally, the encrypted CredentialMetadata was the
// credential ID, and thus all of the contained data was immutable. With
// `Version::kV3` we switched to randomly generated credential IDs, and now
// write the encrypted metadata to a separate attribute, so that it can be
// updated.
struct COMPONENT_EXPORT(DEVICE_FIDO) CredentialMetadata {
 public:
  enum class Version : uint8_t {
    // Initial version.
    kV0 = 0,
    // Added `is_resident`. Previous credentials are non-resident.
    kV1 = 1,
    // Credentials now use a fixed-zero signCounter. Previous credentials
    // encoded timestamps as the signCounter. Also removed the unencrypted
    // version prefix from the credential ID.
    kV2 = 2,
    // Switched to 16-byte random credential IDs. Previous credentials used the
    // encrypted CBOR encoding of `CredentialMetadata` as the credential ID. V3
    // credentials store metadata separate from the credential ID.
    // Also added `uses_timestamp_sign_counter` so we can migrate older
    // instances to V3.
    kV3 = 3,
    // Credentials are created without a `kSecAttrAccessControl` attribute, so
    // that Assertions can now be generated without providing user verification
    // if necessary. Older credentials use a `SecAccessControl` instance with
    // `kSecAccessControlUserPresence` and require the user to pass a device
    // password or Touch ID challenge whenever a signature is produced.
    kV4 = 4,
    // Also update CurrentVersion() when adding values here.
    MAX_VERSION = kV4,
  };

  // Whether the signature counter for the credential is a timestamp or fixed at
  // zero.
  enum class SignCounter : uint8_t {
    kTimestamp = 0,
    kZero = 1,
  };

  // Returns the Version to use for newly created credentials.
  static Version CurrentVersion();

  static CredentialMetadata FromPublicKeyCredentialUserEntity(
      const PublicKeyCredentialUserEntity&,
      bool is_resident);

  CredentialMetadata(Version version,
                     std::vector<uint8_t> user_id,
                     std::string user_name,
                     std::string user_display_name,
                     bool is_resident,
                     SignCounter counter_type);
  CredentialMetadata(const CredentialMetadata&);
  CredentialMetadata(CredentialMetadata&&);
  CredentialMetadata& operator=(const CredentialMetadata&);
  CredentialMetadata& operator=(CredentialMetadata&&);
  ~CredentialMetadata();

  bool operator==(const CredentialMetadata&) const;

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
  bool is_resident = false;

  // The type of signature counter used for this credential.
  //  - V0 and V1 credentials implicitly used a timestamp counter.
  //  - V2 implicitly used a zero counter.
  //  - V3 credentials store this field explicitly, but all newly created
  //    credentials use `SignCounter::kZero`. (This allows metadata for older
  //    credentials (with `SignCounter::kTimestamp`) to be reencoded as V3 on
  //    update.)
  SignCounter sign_counter_type = SignCounter::kZero;
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

// SealCredentialMetadata encodes and encrypts the given CredentialMetadata for
// storage.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> SealCredentialMetadata(const std::string& secret,
                                            const std::string& rp_id,
                                            const CredentialMetadata& metadata);

// UnsealCredentialId attempts to decrypt a CredentialMetadata from a credential
// id for version <= kV2.
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<CredentialMetadata> UnsealMetadataFromLegacyCredentialId(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> credential_id);

// UnsealMetadataFromApplicationTag attempts to decrypt CredentialMetadata from
// an kSecAttrApplicationTag attribute for version >= kV3.
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<CredentialMetadata> UnsealMetadataFromApplicationTag(
    const std::string& secret,
    const std::string& rp_id,
    base::span<const uint8_t> application_tag);

// EncodeRpIdAndUserIdDeprecated encodes the concatenation of RP ID and user ID
// for storage in the macOS keychain.
//
// This encoding allows lookup of credentials for a given RP and user but
// without the credential ID. This is "deprecated" because we're going to
// abandon that encoding for CredentialMetadata v3. Querying by user ID will
// require iterating over all credentials for the RP ID and looking at the
// unsealed metadata.
COMPONENT_EXPORT(DEVICE_FIDO)
std::string EncodeRpIdAndUserIdDeprecated(const std::string& secret,
                                          const std::string& rp_id,
                                          base::span<const uint8_t> user_id);

// EncodeRpId encodes the given RP ID for storage in the macOS keychain. The
// returned value is guaranteed to be a valid UTF-8 string, to ensure it can
// safely be converted to an NSString and used as a string property in a
// parameters dictionary.
COMPONENT_EXPORT(DEVICE_FIDO)
std::string EncodeRpId(const std::string& secret, const std::string& rp_id);

// DecodeRpId attempts to decode a given RP ID from the keychain.
//
// This can be used to test whether a set of credential metadata was created
// under the given secret without knowing the RP ID (which would be required to
// unseal a credential ID).
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<std::string> DecodeRpId(const std::string& secret,
                                      const std::string& ciphertext);

// Seals a legacy V0, V1 or V2 credential ID.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> SealLegacyCredentialIdForTestingOnly(
    CredentialMetadata::Version version,
    const std::string& secret,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    const std::string& user_name,
    const std::string& user_display_name,
    bool is_resident);

}  // namespace fido::mac

}  // namespace device

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
