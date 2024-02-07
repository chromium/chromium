// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ATTESTED_CREDENTIAL_DATA_H_
#define DEVICE_FIDO_ATTESTED_CREDENTIAL_DATA_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "device/fido/fido_constants.h"

namespace device {

struct PublicKey;

// https://www.w3.org/TR/2017/WD-webauthn-20170505/#sec-attestation-data
class COMPONENT_EXPORT(DEVICE_FIDO) AttestedCredentialData {
 public:
  // Parses an |AttestedCredentialData| from a prefix of |*buffer|. Returns
  // nullopt on error, or else the parse return and a (possibly empty) suffix of
  // |buffer| that was not parsed.
  static std::optional<
      std::pair<AttestedCredentialData, base::span<const uint8_t>>>
  ConsumeFromCtapResponse(base::span<const uint8_t> buffer);

  static std::optional<AttestedCredentialData> CreateFromU2fRegisterResponse(
      base::span<const uint8_t> u2f_data,
      std::unique_ptr<PublicKey> public_key);

  AttestedCredentialData(AttestedCredentialData&& other);

  AttestedCredentialData(
      base::span<const uint8_t, kAaguidLength> aaguid,
      base::span<const uint8_t, kCredentialIdLengthLength> credential_id_length,
      std::vector<uint8_t> credential_id,
      std::unique_ptr<PublicKey> public_key);

  // Convenience version of the constructor that automatically constructs
  // `credential_id_length`. `credential_id` size must not exceed 2**16 - 1.
  AttestedCredentialData(base::span<const uint8_t, kAaguidLength> aaguid,
                         base::span<const uint8_t> credential_id,
                         std::unique_ptr<PublicKey> public_key);

  AttestedCredentialData(const AttestedCredentialData&) = delete;
  AttestedCredentialData& operator=(const AttestedCredentialData&) = delete;

  ~AttestedCredentialData();

  AttestedCredentialData& operator=(AttestedCredentialData&& other);

  const std::vector<uint8_t>& credential_id() const { return credential_id_; }

  const std::array<uint8_t, kAaguidLength>& aaguid() const { return aaguid_; }

  // Returns true iff the AAGUID is all zero bytes.
  bool IsAaguidZero() const;

  // Invoked when sending "none" attestation statement to the relying party.
  // Replaces AAGUID with zero bytes. Returns true if the AAGUID was modified
  // or false if it was already zeros.
  bool DeleteAaguid();

  // Produces a byte array consisting of:
  // * AAGUID (16 bytes)
  // * Len (2 bytes)
  // * Credential Id (Len bytes)
  // * Credential Public Key.
  std::vector<uint8_t> SerializeAsBytes() const;

  const PublicKey* public_key() const;

 private:
  // The 16-byte AAGUID of the authenticator.
  std::array<uint8_t, kAaguidLength> aaguid_;

  // Big-endian length of the credential (i.e. key handle).
  std::array<uint8_t, kCredentialIdLengthLength> credential_id_length_;

  std::vector<uint8_t> credential_id_;
  std::unique_ptr<PublicKey> public_key_;
};

}  // namespace device

#endif  // DEVICE_FIDO_ATTESTED_CREDENTIAL_DATA_H_
