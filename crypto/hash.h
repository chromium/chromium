// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_HASH_H_
#define CRYPTO_HASH_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/containers/span.h"
#include "base/notreached.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/digest.h"

namespace base {
class File;
}

namespace crypto::hash {

inline constexpr size_t kSha1Size = 20;
inline constexpr size_t kSha256Size = 32;
inline constexpr size_t kSha384Size = 48;
inline constexpr size_t kSha512Size = 64;

// Unless your code needs to be generic over HashKind, use one of these
// kind-specific functions:
CRYPTO_EXPORT std::array<uint8_t, kSha1Size> Sha1(
    base::span<const uint8_t> data);
CRYPTO_EXPORT std::array<uint8_t, kSha1Size> Sha1(std::string_view data);

CRYPTO_EXPORT std::array<uint8_t, kSha256Size> Sha256(
    base::span<const uint8_t> data);
CRYPTO_EXPORT std::array<uint8_t, kSha256Size> Sha256(std::string_view data);

CRYPTO_EXPORT std::array<uint8_t, kSha512Size> Sha512(
    base::span<const uint8_t> data);
CRYPTO_EXPORT std::array<uint8_t, kSha512Size> Sha512(std::string_view data);

// If you do need to be generic, you can use the Hash() function and pass a
// HashKind instead.
enum HashKind {
  kSha1,
  kSha256,
  kSha384,
  kSha512,
};

// Free functions to convert to/from bssl EVP_MDs. The functions to convert to
// HashKind return optionals because HashKind can only represent a subset of the
// algorithms BoringSSL supports - specifically the ones the //crypto OWNERS
// recommend people use.
CRYPTO_EXPORT const EVP_MD* EVPMDForHashKind(HashKind k);
CRYPTO_EXPORT std::optional<HashKind> HashKindForEVPMD(const EVP_MD* evp_md);

inline constexpr size_t DigestSizeForHashKind(HashKind k) {
  switch (k) {
    case kSha1:
      return kSha1Size;
    case kSha256:
      return kSha256Size;
    case kSha384:
      return kSha384Size;
    case kSha512:
      return kSha512Size;
  }
  NOTREACHED();
}

// One-shot hashing. The passed-in digest span must be the correct size for the
// digest; use DigestSizeForHashKind() if your HashKind is variable.
CRYPTO_EXPORT void Hash(HashKind kind,
                        base::span<const uint8_t> data,
                        base::span<uint8_t> digest);
CRYPTO_EXPORT void Hash(HashKind kind,
                        std::string_view data,
                        base::span<uint8_t> digest);

// A streaming hasher interface. Calling Finish() resets the hash context to the
// initial state after computing the digest.
class CRYPTO_EXPORT Hasher {
 public:
  explicit Hasher(HashKind kind);
  Hasher(const Hasher& other);
  Hasher(Hasher&& other);
  Hasher& operator=(const Hasher& other);
  Hasher& operator=(Hasher&& other);
  ~Hasher();

  void Update(base::span<const uint8_t> data);
  void Update(std::string_view data);

  // The digest span must be the right size. Once Finish() has been called on a
  // Hasher instance, it cannot be used any further: subsequent calls to either
  // Update() or Finish() are illegal and will crash.
  void Finish(base::span<uint8_t> digest);

 private:
  bssl::ScopedEVP_MD_CTX ctx_;
};

// A utility function for a common use-case (hashing the entire body of a
// base::File). The digest span must be of the correct size for the specified
// HashKind. If file IO fails while reading the file, or the passed-in file is
// not valid, the output span is filled with zeroes and this function returns
// false.
[[nodiscard]] CRYPTO_EXPORT bool HashFile(HashKind kind,
                                          base::File* file,
                                          base::span<uint8_t> digest);

}  // namespace crypto::hash

#endif  // CRYPTO_HASH_H_
