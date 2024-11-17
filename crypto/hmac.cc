// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "crypto/hmac.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_util.h"
#include "crypto/symmetric_key.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"

namespace crypto {

HMAC::HMAC(HashAlgorithm hash_alg) : hash_alg_(hash_alg), initialized_(false) {
  // Only SHA-1 and SHA-256 hash algorithms are supported now.
  DCHECK(hash_alg_ == SHA1 || hash_alg_ == SHA256);
}

HMAC::~HMAC() {
  // Zero out key copy.
  key_.assign(key_.size(), 0);
  base::STLClearObject(&key_);
}

size_t HMAC::DigestLength() const {
  switch (hash_alg_) {
    case SHA1:
      return 20;
    case SHA256:
      return 32;
    default:
      NOTREACHED();
  }
}

bool HMAC::Init(const unsigned char* key, size_t key_length) {
  // Init must not be called more than once on the same HMAC object.
  DCHECK(!initialized_);
  initialized_ = true;
  key_.assign(key, key + key_length);
  return true;
}

bool HMAC::Init(const SymmetricKey* key) {
  return Init(key->key());
}

bool HMAC::Sign(std::string_view data,
                unsigned char* digest,
                size_t digest_length) const {
  return Sign(base::as_bytes(base::make_span(data)),
              base::make_span(digest, digest_length));
}

bool HMAC::Sign(base::span<const uint8_t> data,
                base::span<uint8_t> digest) const {
  DCHECK(initialized_);

  if (digest.size() > DigestLength())
    return false;

  ScopedOpenSSLSafeSizeBuffer<EVP_MAX_MD_SIZE> result(digest.data(),
                                                      digest.size());
  return !!::HMAC(hash_alg_ == SHA1 ? EVP_sha1() : EVP_sha256(), key_.data(),
                  key_.size(), data.data(), data.size(), result.safe_buffer(),
                  nullptr);
}

bool HMAC::Verify(std::string_view data, std::string_view digest) const {
  return Verify(base::as_bytes(base::make_span(data)),
                base::as_bytes(base::make_span(digest)));
}

bool HMAC::Verify(base::span<const uint8_t> data,
                  base::span<const uint8_t> digest) const {
  if (digest.size() != DigestLength())
    return false;
  return VerifyTruncated(data, digest);
}

bool HMAC::VerifyTruncated(std::string_view data,
                           std::string_view digest) const {
  return VerifyTruncated(base::as_bytes(base::make_span(data)),
                         base::as_bytes(base::make_span(digest)));
}

bool HMAC::VerifyTruncated(base::span<const uint8_t> data,
                           base::span<const uint8_t> digest) const {
  if (digest.empty())
    return false;

  size_t digest_length = DigestLength();
  if (digest.size() > digest_length)
    return false;

  std::array<uint8_t, EVP_MAX_MD_SIZE> computed_buffer;
  auto computed_digest = base::span(computed_buffer).subspan(0, digest.size());
  if (!Sign(data, computed_digest)) {
    return false;
  }

  return SecureMemEqual(digest, computed_digest);
}

namespace hmac {

namespace {

const EVP_MD* EVPMDForHashKind(crypto::hash::HashKind kind) {
  switch (kind) {
    case crypto::hash::HashKind::kSha1:
      return EVP_sha1();
    case crypto::hash::HashKind::kSha256:
      return EVP_sha256();
    case crypto::hash::HashKind::kSha512:
      return EVP_sha512();
  }
  NOTREACHED();
}

}  // namespace

void Sign(crypto::hash::HashKind kind,
          base::span<const uint8_t> key,
          base::span<const uint8_t> data,
          base::span<uint8_t> hmac) {
  const EVP_MD* md = EVPMDForHashKind(kind);
  CHECK_EQ(hmac.size(), EVP_MD_size(md));

  bssl::ScopedHMAC_CTX ctx;
  CHECK(HMAC_Init_ex(ctx.get(), key.data(), key.size(), EVPMDForHashKind(kind),
                     nullptr));
  CHECK(HMAC_Update(ctx.get(), data.data(), data.size()));
  CHECK(HMAC_Final(ctx.get(), hmac.data(), nullptr));
}

bool Verify(crypto::hash::HashKind kind,
            base::span<const uint8_t> key,
            base::span<const uint8_t> data,
            base::span<const uint8_t> hmac) {
  const EVP_MD* md = EVPMDForHashKind(kind);
  CHECK_EQ(hmac.size(), EVP_MD_size(md));

  std::array<uint8_t, EVP_MAX_MD_SIZE> computed_buf;
  base::span<uint8_t> computed =
      base::span(computed_buf).first(EVP_MD_size(md));

  Sign(kind, key, data, computed);
  return crypto::SecureMemEqual(computed, hmac);
}

std::array<uint8_t, crypto::hash::kSha1Size> SignSha1(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data) {
  std::array<uint8_t, crypto::hash::kSha1Size> result;
  Sign(crypto::hash::HashKind::kSha1, key, data, result);
  return result;
}

std::array<uint8_t, crypto::hash::kSha256Size> SignSha256(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data) {
  std::array<uint8_t, crypto::hash::kSha256Size> result;
  Sign(crypto::hash::HashKind::kSha256, key, data, result);
  return result;
}

std::array<uint8_t, crypto::hash::kSha512Size> SignSha512(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data) {
  std::array<uint8_t, crypto::hash::kSha512Size> result;
  Sign(crypto::hash::HashKind::kSha512, key, data, result);
  return result;
}

bool VerifySha1(base::span<const uint8_t> key,
                base::span<const uint8_t> data,
                base::span<const uint8_t, 20> hmac) {
  return Verify(crypto::hash::HashKind::kSha1, key, data, hmac);
}

bool VerifySha256(base::span<const uint8_t> key,
                  base::span<const uint8_t> data,
                  base::span<const uint8_t, 32> hmac) {
  return Verify(crypto::hash::HashKind::kSha256, key, data, hmac);
}

bool VerifySha512(base::span<const uint8_t> key,
                  base::span<const uint8_t> data,
                  base::span<const uint8_t, 64> hmac) {
  return Verify(crypto::hash::HashKind::kSha512, key, data, hmac);
}

}  // namespace hmac

}  // namespace crypto
