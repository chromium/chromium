// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_OBSOLETE_SHA1_H_
#define CRYPTO_OBSOLETE_SHA1_H_

#include <array>
#include <string_view>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/digest.h"

namespace crypto::obsolete {
static constexpr size_t kSha1Size = 20;
class Sha1;
}  // namespace crypto::obsolete

namespace ash::ambient {
std::string GetCachedImageHash(std::string_view image);
std::string Sha1UrlAsHexEncodeForFilename(std::string_view url);
}  // namespace ash::ambient

namespace crypto::obsolete {

// This class is used for computing SHA-1 hashes, either one-shot via
// SHA1::Hash(), or streaming via constructing a SHA-1 instance, calling
// Update(), then calling Finish(). It cannot be constructed except by friend
// classes, and to become a friend class you must talk to a member of
// //CRYPTO_OWNERS. You should not use SHA-1 in new production code.
class CRYPTO_EXPORT Sha1 {
 public:
  static constexpr size_t kSize = kSha1Size;

  Sha1(const Sha1& other);
  Sha1(Sha1&& other);
  Sha1& operator=(const Sha1& other);
  Sha1& operator=(Sha1&& other);
  ~Sha1();

  void Update(std::string_view data);
  void Update(base::span<const uint8_t> data);

  void Finish(base::span<uint8_t, kSize> result);
  std::array<uint8_t, kSize> Finish();

  static Sha1 MakeSha1HasherForTesting();
  static std::array<uint8_t, kSize> HashForTesting(
      base::span<const uint8_t> data);

 private:
  FRIEND_TEST_ALL_PREFIXES(Sha1Test, KnownAnswer);

  // The friends listed here are the areas required to continue using SHA-1 for
  // compatibility with existing specs, on-disk data, or similar.
  friend std::string ash::ambient::GetCachedImageHash(std::string_view image);
  friend std::string ash::ambient::Sha1UrlAsHexEncodeForFilename(std::string_view url);

  Sha1();
  static std::array<uint8_t, kSize> Hash(std::string_view data);
  static std::array<uint8_t, kSize> Hash(base::span<const uint8_t> data);

  bssl::ScopedEVP_MD_CTX ctx_;
};

}  // namespace crypto::obsolete

#endif  // CRYPTO_OBSOLETE_SHA1_H_
