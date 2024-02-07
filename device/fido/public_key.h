// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_H_
#define DEVICE_FIDO_PUBLIC_KEY_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"

namespace device {

// https://www.w3.org/TR/webauthn/#credentialpublickey
struct COMPONENT_EXPORT(DEVICE_FIDO) PublicKey {
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
