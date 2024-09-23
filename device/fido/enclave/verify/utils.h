// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_UTILS_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/types/expected.h"

namespace device::enclave {

// Makes a plausible guess whether the public key is in PEM format.
bool COMPONENT_EXPORT(DEVICE_FIDO) LooksLikePem(std::string_view maybe_pem);

// Converts a PEM key to raw. Will panic if it does not look like PEM.
base::expected<std::vector<uint8_t>, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    ConvertPemToRaw(std::string_view public_key_pem);

// Converts a raw public key to PEM format.
std::string COMPONENT_EXPORT(DEVICE_FIDO)
    ConvertRawToPem(base::span<const uint8_t> public_key);

// Verifies the signature over the contents using the public key.
base::expected<void, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    VerifySignatureRaw(base::span<const uint8_t> signature,
                       base::span<const uint8_t> contents,
                       base::span<const uint8_t> public_key);

// Compares two DER encoded ECDSA public keys provided. Instead of comparing the bytes, we
// parse the bytes and compare p256 keys. Keys are considered equal if they are the same
// on the elliptic curve. This means that the keys could have different bytes,
// but still be the same key.
base::expected<bool, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    EqualKeys(base::span<const uint8_t> public_key_a,
              base::span<const uint8_t> public_key_b);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_UTILS_H_
