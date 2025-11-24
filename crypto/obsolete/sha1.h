// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_OBSOLETE_SHA1_H_
#define CRYPTO_OBSOLETE_SHA1_H_

#include <array>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/digest.h"

namespace crypto::obsolete {
static constexpr size_t kSha1Size = 20;
class Sha1;
}  // namespace crypto::obsolete

namespace arc {
std::string GetSha1HashForArcPlayTermsOfService(std::string_view tos_content);
}

namespace ash::ambient {
std::string GetCachedImageHash(std::string_view image);
std::string Sha1UrlAsHexEncodeForFilename(std::string_view url);
}  // namespace ash::ambient

namespace ash::login {
std::string GetHashedTosContent(std::string_view tos_content);
std::string Sha1AsHexForRefreshToken(std::string_view data);
}  // namespace ash::login

namespace ash::quick_start {
std::string GetHashedAuthToken(std::string_view authentication_token);
}  // namespace ash::quick_start

namespace kcer::internal {
std::vector<uint8_t> Sha1ForPkcs11Id(base::span<const uint8_t> data);
}  // namespace kcer::internal

namespace net {
std::string ComputeSecWebSocketAccept(std::string_view key);
}  // namespace net

namespace policy {
std::array<uint8_t, crypto::obsolete::kSha1Size> Sha1ForDmTokenFilePath(
    std::string_view input);
std::array<uint8_t, crypto::obsolete::kSha1Size> Sha1ForMachineId(
    std::string_view input);
}  // namespace policy

namespace wallpaper {
std::string GetHexForWallpaperFilesId(
    const base::span<const uint8_t> files_id_unhashed);
}  // namespace wallpaper

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
  friend std::string ash::ambient::Sha1UrlAsHexEncodeForFilename(
      std::string_view url);
  friend std::string ash::login::GetHashedTosContent(
      std::string_view tos_content);
  friend std::string ash::login::Sha1AsHexForRefreshToken(
      std::string_view data);
  friend std::string ash::quick_start::GetHashedAuthToken(
      std::string_view authentication_token);
  friend std::string net::ComputeSecWebSocketAccept(std::string_view key);

  // TODO(crbug.com/457771366): Remove once play_terms_of_service_hash is
  // migrated to use SHA-256.
  friend std::string arc::GetSha1HashForArcPlayTermsOfService(
      std::string_view tos_content);

  // TODO(crbug.com/459863801): get rid of this.
  friend std::vector<uint8_t> kcer::internal::Sha1ForPkcs11Id(
      base::span<const uint8_t> data);

  // TODO(b/460489502): remove once SHA-1 is no longer used for hashing client
  // IDs.
  friend std::array<uint8_t, crypto::obsolete::kSha1Size>
  policy::Sha1ForDmTokenFilePath(std::string_view input);
  friend std::array<uint8_t, crypto::obsolete::kSha1Size>
  policy::Sha1ForMachineId(std::string_view input);

  // TODO(crbug.com/458084930): get rid of this.
  friend std::string wallpaper::GetHexForWallpaperFilesId(
      const base::span<const uint8_t> files_id_unhashed);

  Sha1();
  static std::array<uint8_t, kSize> Hash(std::string_view data);
  static std::array<uint8_t, kSize> Hash(base::span<const uint8_t> data);

  bssl::ScopedEVP_MD_CTX ctx_;
};

}  // namespace crypto::obsolete

#endif  // CRYPTO_OBSOLETE_SHA1_H_
