// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_ENCRYPT_H_
#define CRYPTO_ENCRYPT_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "crypto/keypair.h"

namespace crypto::encrypt {

enum EncryptionKind {
  RSA_OAEP_SHA1,
};

// One-shot encryption function: encrypt `plaintext` using `key`.
CRYPTO_EXPORT std::vector<uint8_t> Encrypt(
    EncryptionKind kind,
    const crypto::keypair::PublicKey& key,
    base::span<const uint8_t> plaintext);

// One-shot decryption function: decrypt `ciphertext` using `key`.
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> Decrypt(
    EncryptionKind kind,
    const crypto::keypair::PrivateKey& key,
    base::span<const uint8_t> ciphertext);

// Returns the ciphertext size for encryption or decryption,
// which corresponds to the modulus size for RSA keys.
CRYPTO_EXPORT size_t GetCiphertextSize(const crypto::keypair::PublicKey& key);
CRYPTO_EXPORT size_t GetCiphertextSize(const crypto::keypair::PrivateKey& key);

// Returns the maximum plaintext size that can be encrypted with the given
// key and algorithm.
CRYPTO_EXPORT size_t GetMaxPlaintextSize(EncryptionKind kind,
                                         const crypto::keypair::PublicKey& key);
CRYPTO_EXPORT size_t
GetMaxPlaintextSize(EncryptionKind kind,
                    const crypto::keypair::PrivateKey& key);

}  // namespace crypto::encrypt

#endif  // CRYPTO_ENCRYPT_H_
