// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hmac.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_util.h"
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

bool HMAC::Init(base::span<const uint8_t> key) {
  // Init must not be called more than once on the same HMAC object.
  DCHECK(!initialized_);
  initialized_ = true;
  key_ = base::ToVector(key);
  return true;
}

bool HMAC::Sign(std::string_view data,
                unsigned char* digest,
                size_t digest_length) const {
  return Sign(base::as_byte_span(data),
              UNSAFE_TODO(base::span(digest, digest_length)));
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
  return Verify(base::as_byte_span(data), base::as_byte_span(digest));
}

bool HMAC::Verify(base::span<const uint8_t> data,
                  base::span<const uint8_t> digest) const {
  std::array<uint8_t, EVP_MAX_MD_SIZE> computed_buffer;
  auto computed_digest = base::span(computed_buffer).first(DigestLength());
  if (!Sign(data, computed_digest)) {
    return false;
  }

  return SecureMemEqual(digest, computed_digest);
}

namespace hmac {

void Sign(crypto::hash::HashKind kind,
          base::span<const uint8_t> key,
          base::span<const uint8_t> data,
          base::span<uint8_t> hmac) {
  const EVP_MD* md = crypto::hash::EVPMDForHashKind(kind);
  CHECK_EQ(hmac.size(), EVP_MD_size(md));

  bssl::ScopedHMAC_CTX ctx;
  CHECK(HMAC_Init_ex(ctx.get(), key.data(), key.size(), md, nullptr));
  CHECK(HMAC_Update(ctx.get(), data.data(), data.size()));
  CHECK(HMAC_Final(ctx.get(), hmac.data(), nullptr));
}

bool Verify(crypto::hash::HashKind kind,
            base::span<const uint8_t> key,
            base::span<const uint8_t> data,
            base::span<const uint8_t> hmac) {
  const EVP_MD* md = crypto::hash::EVPMDForHashKind(kind);
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

HmacSigner::HmacSigner(crypto::hash::HashKind kind,
                       base::span<const uint8_t> key)
    : kind_(kind), finished_(false) {
  CHECK(HMAC_Init_ex(ctx_.get(), key.data(), key.size(),
                     crypto::hash::EVPMDForHashKind(kind), nullptr));
}

HmacSigner::~HmacSigner() = default;

void HmacSigner::Update(base::span<const uint8_t> data) {
  CHECK(!finished_);
  CHECK(HMAC_Update(ctx_.get(), data.data(), data.size()));
}

void HmacSigner::Finish(base::span<uint8_t> result) {
  CHECK(!finished_);
  finished_ = true;
  unsigned int len = result.size();
  CHECK(HMAC_Final(ctx_.get(), result.data(), &len));
  CHECK(len == result.size());
}

std::vector<uint8_t> HmacSigner::Finish() {
  std::vector<uint8_t> result(crypto::hash::DigestSizeForHashKind(kind_));
  Finish(result);
  return result;
}

HmacVerifier::HmacVerifier(crypto::hash::HashKind kind,
                           base::span<const uint8_t> key)
    : signer_(kind, key) {}
HmacVerifier::~HmacVerifier() = default;

void HmacVerifier::Update(base::span<const uint8_t> data) {
  signer_.Update(data);
}

bool HmacVerifier::Finish(base::span<const uint8_t> expected_signature) {
  std::vector<uint8_t> result = signer_.Finish();
  return crypto::SecureMemEqual(result, expected_signature);
}

}  // namespace hmac

}  // namespace crypto
