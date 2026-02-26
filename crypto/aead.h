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
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace crypto {

namespace aead {

enum Algorithm {
  AES_128_CTR_HMAC_SHA256,
  AES_256_GCM,
  AES_256_GCM_SIV,
  CHACHA20_POLY1305
};

CRYPTO_EXPORT size_t KeySizeFor(Algorithm algorithm);
CRYPTO_EXPORT size_t NonceSizeFor(Algorithm algorithm);

// One-shot AEAD interfaces; prefer these over the stateful one unless you
// compute multiple AEADs with the same key. These CHECK that the key and nonce
// given are of the right size for the algorithm provided.
CRYPTO_EXPORT std::vector<uint8_t> Seal(
    Algorithm algorithm,
    base::span<const uint8_t> key,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> associated_data);
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> Open(
    Algorithm algorithm,
    base::span<const uint8_t> key,
    base::span<const uint8_t> ciphertext,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> associated_data);

}  // namespace aead

// This class exposes the AES-128-CTR-HMAC-SHA256 and AES_256_GCM AEAD. Note
// that there are two versions of most methods: an historical version based
// around |std::string_view| and a more modern version that takes |base::span|.
// Prefer the latter in new code.
class CRYPTO_EXPORT Aead {
 public:
  // These allow older client code that assumed the members of this enum were
  // part of this class to continue working as before.
  using AeadAlgorithm = aead::Algorithm;
  static constexpr auto AES_128_CTR_HMAC_SHA256 = aead::AES_128_CTR_HMAC_SHA256;
  static constexpr auto AES_256_GCM = aead::AES_256_GCM;
  static constexpr auto AES_256_GCM_SIV = aead::AES_256_GCM_SIV;
  static constexpr auto CHACHA20_POLY1305 = aead::CHACHA20_POLY1305;

  // If you use the one-arg form here, you must call Init() to configure a key.
  // TODO(https://crbug.com/475891208): remove this; there are no callers (nor
  // is there any reason) to construct an Aead instance before the key is
  // available.
  explicit Aead(AeadAlgorithm algorithm);

  // This CHECKs that the passed-in key is of the right length for the passed-in
  // algorithm.
  Aead(AeadAlgorithm algorithm, base::span<const uint8_t> key);

  Aead(const Aead&) = delete;
  Aead& operator=(const Aead&) = delete;
  ~Aead();

  // These are only legal to call if the key was not supplied at construction
  // time. The key is copied locally and stored inside |this|.
  //
  // If the key is of the wrong size for the specified algorithm, or the
  // receiving object has not been Init()ed, then Seal() and Open() always fail.
  //
  // TODO(https://crbug.com/475891208): remove this.
  void Init(base::span<const uint8_t> key);
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

  bssl::ScopedEVP_AEAD_CTX ctx_;

  // It should not be necessary to store this; we only need it to support
  // two-phase construct-init which is itself deprecated.
  // TODO(https://crbug.com/475891208): remove this
  AeadAlgorithm algorithm_;

  // Whether Init() succeeded, in which case other methods can be used.
  // Temporary workaround.
  // TODO(https://crbug.com/478966624): remove this
  bool initialized_{false};
};

}  // namespace crypto

#endif  // CRYPTO_AEAD_H_
