// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_AEAD_H_
#define CRYPTO_AEAD_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "crypto/crypto_export.h"

struct evp_aead_st;

namespace crypto {

// This class exposes the AES-128-CTR-HMAC-SHA256 and AES_256_GCM AEAD. Note
// that there are two versions of most methods: an historical version based
// around |StringPiece| and a more modern version that takes |base::span|.
// Prefer the latter in new code.
class CRYPTO_EXPORT Aead {
 public:
  enum AeadAlgorithm { AES_128_CTR_HMAC_SHA256, AES_256_GCM, AES_256_GCM_SIV };

  explicit Aead(AeadAlgorithm algorithm);
  Aead(const Aead&) = delete;
  Aead& operator=(const Aead&) = delete;
  ~Aead();

  // Note that Init keeps a reference to the data pointed to by |key| thus that
  // data must outlive this object.
  void Init(base::span<const uint8_t> key);

  // Note that Init keeps a reference to the data pointed to by |key| thus that
  // data must outlive this object.
  void Init(const std::string* key);

  std::vector<uint8_t> Seal(base::span<const uint8_t> plaintext,
                            base::span<const uint8_t> nonce,
                            base::span<const uint8_t> additional_data) const;

  bool Seal(base::StringPiece plaintext,
            base::StringPiece nonce,
            base::StringPiece additional_data,
            std::string* ciphertext) const;

  base::Optional<std::vector<uint8_t>> Open(
      base::span<const uint8_t> ciphertext,
      base::span<const uint8_t> nonce,
      base::span<const uint8_t> additional_data) const;

  bool Open(base::StringPiece ciphertext,
            base::StringPiece nonce,
            base::StringPiece additional_data,
            std::string* plaintext) const;

  size_t KeyLength() const;

  size_t NonceLength() const;

 private:
  bool Seal(base::span<const uint8_t> plaintext,
            base::span<const uint8_t> nonce,
            base::span<const uint8_t> additional_data,
            uint8_t* out,
            size_t* output_length,
            size_t max_output_length) const;

  bool Open(base::span<const uint8_t> ciphertext,
            base::span<const uint8_t> nonce,
            base::span<const uint8_t> additional_data,
            uint8_t* out,
            size_t* output_length,
            size_t max_output_length) const;

  base::Optional<base::span<const uint8_t>> key_;
  const evp_aead_st* aead_;
};

}  // namespace crypto

#endif  // CRYPTO_AEAD_H_
