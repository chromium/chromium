// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility class for calculating the HMAC for a given message. We currently only
// support SHA-1 and SHA-256 for the hash algorithm, but this can be extended
// easily. Prefer the base::span and std::vector overloads over the
// std::string_view and std::string overloads.

#ifndef CRYPTO_HMAC_H_
#define CRYPTO_HMAC_H_

#include <stddef.h>

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "crypto/hash.h"
#include "crypto/secure_util.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"

namespace crypto {

// TODO(https://issues.chromium.org/issues/374334448): Rework this interface and
// delete much of it.
// Deprecated; don't add new uses. See the interfaces below this class instead.
class CRYPTO_EXPORT HMAC {
 public:
  // The set of supported hash functions. Extend as required.
  enum HashAlgorithm {
    SHA1,
    SHA256,
  };

  explicit HMAC(HashAlgorithm hash_alg);

  HMAC(const HMAC&) = delete;
  HMAC& operator=(const HMAC&) = delete;

  ~HMAC();

  // Returns the length of digest that this HMAC will create.
  size_t DigestLength() const;

  // TODO(abarth): Add a PreferredKeyLength() member function.

  // Initializes this instance using |key|. Call Init only once. It returns
  // false on the second or later calls.
  [[nodiscard]] bool Init(std::string_view key) {
    return Init(base::as_byte_span(key));
  }

  // Initializes this instance using |key|. Call Init only once. It returns
  // false on the second or later calls.
  [[nodiscard]] bool Init(base::span<const uint8_t> key);

  // Calculates the HMAC for the message in |data| using the algorithm supplied
  // to the constructor and the key supplied to the Init method. The HMAC is
  // returned in |digest|, which has |digest_length| bytes of storage available.
  // If |digest_length| is smaller than DigestLength(), the output will be
  // truncated. If it is larger, this method will fail.
  [[nodiscard]] bool Sign(std::string_view data,
                          unsigned char* digest,
                          size_t digest_length) const;
  [[nodiscard]] bool Sign(base::span<const uint8_t> data,
                          base::span<uint8_t> digest) const;

  // Verifies that the HMAC for the message in |data| equals the HMAC provided
  // in |digest|, using the algorithm supplied to the constructor and the key
  // supplied to the Init method. Use of this method is strongly recommended
  // over using Sign() with a manual comparison (such as memcmp), as such
  // comparisons may result in side-channel disclosures, such as timing, that
  // undermine the cryptographic integrity. |digest| must be exactly
  // |DigestLength()| bytes long.
  [[nodiscard]] bool Verify(std::string_view data,
                            std::string_view digest) const;
  [[nodiscard]] bool Verify(base::span<const uint8_t> data,
                            base::span<const uint8_t> digest) const;

 private:
  HashAlgorithm hash_alg_;
  bool initialized_;
  std::vector<unsigned char> key_;
};

namespace hmac {

// Single-shot interfaces for working with HMACs. Unless your code needs to be
// generic over hash kinds, you should use the convenience interfaces that are
// named after a specific kind, since they allow compile-time error checking of
// the hmac size.
CRYPTO_EXPORT std::array<uint8_t, crypto::hash::kSha1Size> SignSha1(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data);

CRYPTO_EXPORT std::array<uint8_t, crypto::hash::kSha256Size> SignSha256(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data);

CRYPTO_EXPORT std::array<uint8_t, crypto::hash::kSha512Size> SignSha512(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data);

[[nodiscard]] CRYPTO_EXPORT bool VerifySha1(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data,
    base::span<const uint8_t, crypto::hash::kSha1Size> hmac);

[[nodiscard]] CRYPTO_EXPORT bool VerifySha256(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data,
    base::span<const uint8_t, crypto::hash::kSha256Size> hmac);

[[nodiscard]] CRYPTO_EXPORT bool VerifySha512(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data,
    base::span<const uint8_t, crypto::hash::kSha512Size> hmac);

// If you need to be generic over hash types, you can instead use these, but you
// must pass the correct size buffer for |hmac|:
CRYPTO_EXPORT void Sign(crypto::hash::HashKind kind,
                        base::span<const uint8_t> key,
                        base::span<const uint8_t> data,
                        base::span<uint8_t> hmac);
[[nodiscard]] CRYPTO_EXPORT bool Verify(crypto::hash::HashKind kind,
                                        base::span<const uint8_t> key,
                                        base::span<const uint8_t> data,
                                        base::span<const uint8_t> hmac);

// Streaming sign and verify interfaces. In general you should only use these if
// you are taking the HMAC of multiple chunks of data and want to avoid making
// an intermediate copy - otherwise the one-shot interfaces are simpler to use.
//
// These classes don't impose any requirements on key sizes.
//
// After you call Finish() on an instance of these classes, it is illegal to
// call Update() or Finish() on it again.
class CRYPTO_EXPORT HmacSigner {
 public:
  HmacSigner(crypto::hash::HashKind kind, base::span<const uint8_t> key);
  ~HmacSigner();

  void Update(base::span<const uint8_t> data);
  void Finish(base::span<uint8_t> result);
  std::vector<uint8_t> Finish();

 private:
  const crypto::hash::HashKind kind_;
  bool finished_;
  bssl::ScopedHMAC_CTX ctx_;
};

class CRYPTO_EXPORT HmacVerifier {
 public:
  HmacVerifier(crypto::hash::HashKind kind, base::span<const uint8_t> key);
  ~HmacVerifier();

  void Update(base::span<const uint8_t> data);

  // Returns whether the signature of all the data passed in via Update() so far
  // matches |expected_signature|. This function tolerates the expected
  // signature being the wrong length (by returning false in that case).
  [[nodiscard]] bool Finish(base::span<const uint8_t> expected_signature);

 private:
  HmacSigner signer_;
};

}  // namespace hmac

}  // namespace crypto

#endif  // CRYPTO_HMAC_H_
