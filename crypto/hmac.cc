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

  uint8_t computed_digest[EVP_MAX_MD_SIZE];
  CHECK_LE(digest.size(), size_t{EVP_MAX_MD_SIZE});
  if (!Sign(data, base::make_span(computed_digest, digest.size())))
    return false;

  return SecureMemEqual(digest.data(), computed_digest, digest.size());
}

}  // namespace crypto
