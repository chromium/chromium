// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_AEAD_H_
#define CRYPTO_AEAD_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "crypto/crypto_export.h"

struct evp_aead_st;

namespace crypto {

// This class exposes the AES-128-CTR-HMAC-SHA256 and AES_256_GCM AEAD. Note
// that there are two versions of most methods: an historical version based
// around |std::string_view| and a more modern version that takes |base::span|.
// Prefer the latter in new code.
class CRYPTO_EXPORT Aead {
 public:
  enum AeadAlgorithm {
    AES_128_CTR_HMAC_SHA256,
    AES_256_GCM,
    AES_256_GCM_SIV,
    CHACHA20_POLY1305
  };

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

  bool Seal(std::string_view plaintext,
            std::string_view nonce,
            std::string_view additional_data,
            std::string* ciphertext) const;

  std::optional<std::vector<uint8_t>> Open(
      base::span<const uint8_t> ciphertext,
      base::span<const uint8_t> nonce,
      base::span<const uint8_t> additional_data) const;

  bool Open(std::string_view ciphertext,
            std::string_view nonce,
            std::string_view additional_data,
            std::string* plaintext) const;

  size_t KeyLength() const;

  size_t NonceLength() const;

 private:
  std::optional<size_t> Seal(base::span<const uint8_t> plaintext,
                             base::span<const uint8_t> nonce,
                             base::span<const uint8_t> additional_data,
                             base::span<uint8_t> out) const;

  std::optional<size_t> Open(base::span<const uint8_t> ciphertext,
                             base::span<const uint8_t> nonce,
                             base::span<const uint8_t> additional_data,
                             base::span<uint8_t> out) const;

  std::optional<base::raw_span<const uint8_t, DanglingUntriaged>> key_;
  raw_ptr<const evp_aead_st> aead_;
};

}  // namespace crypto

#endif  // CRYPTO_AEAD_H_
