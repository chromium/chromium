// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_AES_CTR_H_
#define CRYPTO_AES_CTR_H_

#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"

namespace crypto::aes_ctr {

inline constexpr size_t kCounterSize = 16;

// Single-shot encryption and decryption operations. These require that the
// output span be the same size as the input span, cannot fail, and do not
// handle incrementing the counter for you. These can either operate in-place
// (meaning in == out) or on entirely disjoint in and out buffers, but *not* on
// overlapping-but-unequal in and out buffers.
//
// Crypto note: It is VERY UNSAFE to encrypt two different messages using the
// same key and counter in this mode - you will leak the key stream and
// thereafter both plaintexts.
//
// Note: in theory it would be nicer to have a proper stateful API for this, but
// in practive every client of raw CTR encryption in Chromium does single-shot
// operations and throws away the counter value afterwards, so such complexity
// would be wasted.

CRYPTO_EXPORT void Encrypt(base::span<const uint8_t> key,
                           base::span<const uint8_t, kCounterSize> counter,
                           base::span<const uint8_t> in,
                           base::span<uint8_t> out);

CRYPTO_EXPORT void Decrypt(base::span<const uint8_t> key,
                           base::span<const uint8_t, kCounterSize> counter,
                           base::span<const uint8_t> in,
                           base::span<uint8_t> out);

// If it's more convenient, there are also wrappers that allocate a byte vector
// for the result for you:

CRYPTO_EXPORT std::vector<uint8_t> Encrypt(
    base::span<const uint8_t> key,
    base::span<const uint8_t, kCounterSize> iv,
    base::span<const uint8_t> in);

CRYPTO_EXPORT std::vector<uint8_t> Decrypt(
    base::span<const uint8_t> key,
    base::span<const uint8_t, kCounterSize> iv,
    base::span<const uint8_t> in);

}  // namespace crypto::aes_ctr

#endif  // CRYPTO_AES_CTR_H_
