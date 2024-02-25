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

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"

namespace crypto {

// Simplify the interface and reduce includes by abstracting out the internals.
class SymmetricKey;

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

  // Initializes this instance using |key| of the length |key_length|. Call Init
  // only once. It returns false on the second or later calls.
  //
  // NOTE: the US Federal crypto standard FIPS 198, Section 3 says:
  //   The size of the key, K, shall be equal to or greater than L/2, where L
  //   is the size of the hash function output.
  // In FIPS 198-1 (and SP-800-107, which describes key size recommendations),
  // this requirement is gone.  But a system crypto library may still enforce
  // this old requirement.  If the key is shorter than this recommended value,
  // Init() may fail.
  [[nodiscard]] bool Init(const unsigned char* key, size_t key_length);

  // Initializes this instance using |key|. Call Init
  // only once. It returns false on the second or later calls.
  [[nodiscard]] bool Init(const SymmetricKey* key);

  // Initializes this instance using |key|. Call Init only once. It returns
  // false on the second or later calls.
  [[nodiscard]] bool Init(std::string_view key) {
    return Init(base::as_bytes(base::make_span(key)));
  }

  // Initializes this instance using |key|. Call Init only once. It returns
  // false on the second or later calls.
  [[nodiscard]] bool Init(base::span<const uint8_t> key) {
    return Init(key.data(), key.size());
  }

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

  // Verifies a truncated HMAC, behaving identical to Verify(), except
  // that |digest| is allowed to be smaller than |DigestLength()|.
  [[nodiscard]] bool VerifyTruncated(std::string_view data,
                                     std::string_view digest) const;
  [[nodiscard]] bool VerifyTruncated(base::span<const uint8_t> data,
                                     base::span<const uint8_t> digest) const;

 private:
  HashAlgorithm hash_alg_;
  bool initialized_;
  std::vector<unsigned char> key_;
};

}  // namespace crypto

#endif  // CRYPTO_HMAC_H_
