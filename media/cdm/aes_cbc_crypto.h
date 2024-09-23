// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_AES_CBC_CRYPTO_H_
#define MEDIA_CDM_AES_CBC_CRYPTO_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "media/base/media_export.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto {
class SymmetricKey;
}

namespace media {

// This class implements AES-CBC-128 decryption as described in the Advanced
// Encryption Standard specified by AES [FIPS-197, https://www.nist.gov]
// using 128-bit keys in Cipher Block Chaining mode, as specified in Block
// Cipher Modes [NIST 800-38A, https://www.nist.gov].

class MEDIA_EXPORT AesCbcCrypto {
 public:
  AesCbcCrypto();

  AesCbcCrypto(const AesCbcCrypto&) = delete;
  AesCbcCrypto& operator=(const AesCbcCrypto&) = delete;

  ~AesCbcCrypto();

  // Initializes the encryptor using |key| and |iv|. Returns false if either
  // the key or the initialization vector cannot be used.
  bool Initialize(const crypto::SymmetricKey& key,
                  base::span<const uint8_t> iv);

  // Decrypts |encrypted_data| into |decrypted_data|. |encrypted_data| must be
  // a multiple of the blocksize (128 bits), and |decrypted_data| must have
  // enough space for |encrypted_data|.size(). Returns false if the decryption
  // fails.
  bool Decrypt(base::span<const uint8_t> encrypted_data,
               uint8_t* decrypted_data);

 private:
  bssl::ScopedEVP_CIPHER_CTX ctx_;
};

}  // namespace media

#endif  // MEDIA_CDM_AES_CBC_CRYPTO_H_
