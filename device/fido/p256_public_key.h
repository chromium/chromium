// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_P256_PUBLIC_KEY_H_
#define DEVICE_FIDO_P256_PUBLIC_KEY_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/cbor/values.h"

namespace device {

struct PublicKey;

struct COMPONENT_EXPORT(DEVICE_FIDO) P256PublicKey {
  static std::unique_ptr<PublicKey> ExtractFromU2fRegistrationResponse(
      int32_t algorithm,
      base::span<const uint8_t> u2f_data);

  static std::unique_ptr<PublicKey> ExtractFromCOSEKey(
      int32_t algorithm,
      base::span<const uint8_t> cbor_bytes,
      const cbor::Value::MapValue& map);

  // Parse a public key encoded in ANSI X9.62 uncompressed format.
  static std::unique_ptr<PublicKey> ParseX962Uncompressed(
      int32_t algorithm,
      base::span<const uint8_t> input);

  // Parse a public key from a DER-encoded X.509 SubjectPublicKeyInfo.
  static std::unique_ptr<PublicKey> ParseSpkiDer(
      int32_t algorithm,
      base::span<const uint8_t> input);
};

}  // namespace device

#endif  // DEVICE_FIDO_P256_PUBLIC_KEY_H_
