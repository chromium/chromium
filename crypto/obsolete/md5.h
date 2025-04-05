// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_OBSOLETE_MD5_H_
#define CRYPTO_OBSOLETE_MD5_H_

#include <array>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::obsolete {

// This class is used for computing MD5 hashes, either one-shot via Md5::Hash(),
// or streaming via constructing an Md5 instance, calling Update(), then calling
// Finish(). It cannot be constructed except by friend classes, and to become a
// friend class you must talk to a member of //CRYPTO_OWNERS. You should not use
// MD5 in new production code.
class CRYPTO_EXPORT Md5 {
 public:
  static constexpr size_t kSize = 16;

  ~Md5();

  void Update(base::span<const uint8_t> data);

  void Finish(base::span<uint8_t, kSize> result);

 private:
  FRIEND_TEST_ALL_PREFIXES(Md5Test, KnownAnswer);

  Md5();
  static std::array<uint8_t, kSize> Hash(base::span<const uint8_t> data);

  bssl::ScopedEVP_MD_CTX ctx_;
};

}  // namespace crypto::obsolete

#endif  // CRYPTO_OBSOLETE_MD5_H_
