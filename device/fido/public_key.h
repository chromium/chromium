// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_H_
#define DEVICE_FIDO_PUBLIC_KEY_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/cbor/values.h"

namespace device {

// https://www.w3.org/TR/webauthn/#credentialpublickey
struct COMPONENT_EXPORT(DEVICE_FIDO) PublicKey {
  // FromCOSEKey parses a public key from a COSE_Key CBOR map. Returns nullptr
  // if the key is invalid or corrupted. If the key type is unknown/unsupported,
  // returns a PublicKey with nullopt der_bytes.
  static std::unique_ptr<PublicKey> FromCOSEKey(
      int32_t algorithm,
      base::span<const uint8_t> cbor_bytes,
      const cbor::Value::MapValue& map);

  // FromSpkiDer parses an ASN.1, DER, SubjectPublicKeyInfo.
  static std::unique_ptr<PublicKey> FromSpkiDer(
      int32_t algorithm,
      base::span<const uint8_t> spki_der);

  // FromU2fRegistrationResponse parses the public key from a U2F registration
  // response (reserved byte prefix + 65-byte uncompressed X9.62 point).
  static std::unique_ptr<PublicKey> FromU2fRegistrationResponse(
      int32_t algorithm,
      base::span<const uint8_t> u2f_data);

  // FromRawP256UncompressedPoint parses a 65-byte uncompressed X9.62 point for
  // P-256 (0x04 || X || Y).
  static std::unique_ptr<PublicKey> FromRawP256UncompressedPoint(
      int32_t algorithm,
      base::span<const uint8_t> x962);

  PublicKey(int32_t algorithm,
            base::span<const uint8_t> cbor_bytes,
            std::optional<std::vector<uint8_t>> der_bytes);

  PublicKey(const PublicKey&) = delete;
  PublicKey& operator=(const PublicKey&) = delete;

  ~PublicKey();

  // algorithm contains the COSE algorithm identifier for this public key.
  const int32_t algorithm;

  // cose_key_bytes contains the credential public key as a COSE_Key map as
  // defined in Section 7 of https://tools.ietf.org/html/rfc8152.
  const std::vector<uint8_t> cose_key_bytes;

  // der_bytes contains an ASN.1, DER, SubjectPublicKeyInfo describing this
  // public key, if possible. (WebAuthn can negotiate the use of unknown
  // public-key algorithms so not all public keys can be transformed into SPKI
  // form.)
  const std::optional<std::vector<uint8_t>> der_bytes;
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_H_
