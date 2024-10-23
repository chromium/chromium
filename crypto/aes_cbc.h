// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"

#ifndef CRYPTO_AES_CBC_H_
#define CRYPTO_AES_CBC_H_

// These functions implement one-shot block cipher encrypt/decrypt operations
// for AES-CBC. This interface is deliberately not abstracted over cipher type -
// new code should prefer the higher-level AEAD interface instead.
namespace crypto::aes_cbc {

inline constexpr size_t kBlockSize = 16;

// WARNING: In general you should not use these, and should prefer an AEAD mode
// which includes authentication.
//
// * The key must be 16 or 32 bytes long, for AES-128 or AES-256 respectively.
// * Decrypt() can fail if padding is incorrect, in which case it returns
//   nullopt.
//
// Design note:
// It may at first seem appealing to replace these functions with equivalents
// that take out parameters to avoid allocating a new value, but it is fiddly
// for the caller to compute the size of the output buffer correctly and for
// Encrypt() to ensure that junk data is not left in the buffer afterwards. For
// example, one could do:
//   size_t Encrypt(&[u8] key, &[u8] iv, &[u8] plaintext, &mut [u8] ciphertext)
// but then the caller could accidentally discard the size and use the full
// ciphertext buffer, even if not all of it was written. It's simpler to just
// always do a heap allocation here, and let callers that care about avoiding it
// use the BoringSSL APIs directly.
//
// WARNING: Do not call Decrypt() with an unauthenticated ciphertext, because
// you are very likely to accidentally create a padding oracle.
CRYPTO_EXPORT std::vector<uint8_t> Encrypt(
    base::span<const uint8_t> key,
    base::span<const uint8_t, kBlockSize> iv,
    base::span<const uint8_t> plaintext);
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> Decrypt(
    base::span<const uint8_t> key,
    base::span<const uint8_t, kBlockSize> iv,
    base::span<const uint8_t> ciphertext);

}  // namespace crypto::aes_cbc

#endif  // CRYPTO_AES_CBC_H_
