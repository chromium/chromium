// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_DATA_H_
#define DEVICE_FIDO_AUTHENTICATOR_DATA_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/fido_constants.h"

namespace device {

// https://www.w3.org/TR/2017/WD-webauthn-20170505/#sec-authenticator-data.
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorData {
 public:
  enum class Flag : uint8_t {
    kTestOfUserPresence = 1u << 0,
    kTestOfUserVerification = 1u << 2,
    kAttestation = 1u << 6,
    kExtensionDataIncluded = 1u << 7,
  };

  static base::Optional<AuthenticatorData> DecodeAuthenticatorData(
      base::span<const uint8_t> auth_data);

  //  The attested credential |data| must be specified iff |flags| have
  //  kAttestation set; and |extensions| must be specified iff |flags| have
  //  kExtensionDataIncluded set.
  AuthenticatorData(
      base::span<const uint8_t, kRpIdHashLength> application_parameter,
      uint8_t flags,
      base::span<const uint8_t, kSignCounterLength> counter,
      base::Optional<AttestedCredentialData> data,
      base::Optional<cbor::Value> extensions = base::nullopt);

  // Moveable.
  AuthenticatorData(AuthenticatorData&& other);
  AuthenticatorData& operator=(AuthenticatorData&& other);

  ~AuthenticatorData();

  // Replaces device AAGUID in attested credential data section with zeros.
  // https://w3c.github.io/webauthn/#attested-credential-data
  void DeleteDeviceAaguid();

  // Produces a byte array consisting of:
  // * hash(relying_party_id / appid)
  // * flags
  // * counter
  // * attestation_data.
  std::vector<uint8_t> SerializeToByteArray() const;

  // Retrieve credential ID from attested credential data section of the
  // authenticator data.
  std::vector<uint8_t> GetCredentialId() const;

  const base::Optional<AttestedCredentialData>& attested_data() const {
    return attested_data_;
  }

  // If a value is returned then the result of calling |is_map()| on it can be
  // assumed to be true.
  const base::Optional<cbor::Value>& extensions() const { return extensions_; }

  const std::array<uint8_t, kRpIdHashLength>& application_parameter() const {
    return application_parameter_;
  }

  bool obtained_user_presence() const {
    return flags_ & base::strict_cast<uint8_t>(Flag::kTestOfUserPresence);
  }

  bool obtained_user_verification() const {
    return flags_ & base::strict_cast<uint8_t>(Flag::kTestOfUserVerification);
  }

  bool attestation_credential_included() const {
    return flags_ & base::strict_cast<uint8_t>(Flag::kAttestation);
  }

  bool extension_data_included() const {
    return flags_ & base::strict_cast<uint8_t>(Flag::kExtensionDataIncluded);
  }

  base::span<const uint8_t, kSignCounterLength> counter() const {
    return counter_;
  }

 private:
  // The application parameter: a SHA-256 hash of either the RP ID or the AppID
  // associated with the credential.
  std::array<uint8_t, kRpIdHashLength> application_parameter_;

  // Flags (bit 0 is the least significant bit):
  // [ED | AT | RFU | RFU | RFU | RFU | RFU | UP ]
  //  * Bit 0: Test of User Presence (TUP) result.
  //  * Bits 1-5: Reserved for future use (RFU).
  //  * Bit 6: Attestation data included (AT).
  //  * Bit 7: Extension data included (ED).
  uint8_t flags_;

  // Signature counter, 32-bit unsigned big-endian integer.
  std::array<uint8_t, kSignCounterLength> counter_;
  base::Optional<AttestedCredentialData> attested_data_;
  // If |extensions_| has a value, then it will be a CBOR map.
  base::Optional<cbor::Value> extensions_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorData);
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_DATA_H_
