// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_ECDSA_UTILS_H_
#define CRYPTO_ECDSA_UTILS_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"

namespace crypto {

namespace keypair {
class PublicKey;
}

// Converts a DER-encoded ECDSA-Sig-Value signature to the fixed-width format
// defined in IEEE P1363
// (https://commondatastorage.googleapis.com/chromium-boringssl-docs/ecdsa.h.html#IEEE-P1363-signing-and-verifying).
// In it, signatures are a concatenation of the big-endian padded `r` and `s`
// components. The length of `r` and `s` is determined by the curve of the
// public key.
//
// This format is used in particular in JWT.
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> ConvertEcdsaDerSignatureToRaw(
    const keypair::PublicKey& public_key,
    base::span<const uint8_t> der_signature);

}  // namespace crypto

#endif  // CRYPTO_ECDSA_UTILS_H_
