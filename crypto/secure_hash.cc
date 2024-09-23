// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/secure_hash.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace crypto {

namespace {

class SecureHashSHA256 : public SecureHash {
 public:
  SecureHashSHA256() { SHA256_Init(&ctx_); }

  SecureHashSHA256(const SecureHashSHA256& other) {
    memcpy(&ctx_, &other.ctx_, sizeof(ctx_));
  }

  ~SecureHashSHA256() override {
    OPENSSL_cleanse(&ctx_, sizeof(ctx_));
  }

  void Update(base::span<const uint8_t> input) override {
    SHA256_Update(&ctx_, input.data(), input.size());
  }

  void Finish(base::span<uint8_t> output) override {
    ScopedOpenSSLSafeSizeBuffer<SHA256_DIGEST_LENGTH> result(output.data(),
                                                             output.size());
    SHA256_Final(result.safe_buffer(), &ctx_);
  }

  std::unique_ptr<SecureHash> Clone() const override {
    return std::make_unique<SecureHashSHA256>(*this);
  }

  size_t GetHashLength() const override { return SHA256_DIGEST_LENGTH; }

 private:
  SHA256_CTX ctx_;
};

class SecureHashSHA512 : public SecureHash {
 public:
  SecureHashSHA512() { SHA512_Init(&ctx_); }

  SecureHashSHA512(const SecureHashSHA512& other) {
    memcpy(&ctx_, &other.ctx_, sizeof(ctx_));
  }

  ~SecureHashSHA512() override { OPENSSL_cleanse(&ctx_, sizeof(ctx_)); }

  void Update(base::span<const uint8_t> input) override {
    SHA512_Update(&ctx_, input.data(), input.size());
  }

  void Finish(base::span<uint8_t> output) override {
    ScopedOpenSSLSafeSizeBuffer<SHA512_DIGEST_LENGTH> result(output.data(),
                                                             output.size());
    SHA512_Final(result.safe_buffer(), &ctx_);
  }

  std::unique_ptr<SecureHash> Clone() const override {
    return std::make_unique<SecureHashSHA512>(*this);
  }

  size_t GetHashLength() const override { return SHA512_DIGEST_LENGTH; }

 private:
  SHA512_CTX ctx_;
};

}  // namespace

std::unique_ptr<SecureHash> SecureHash::Create(Algorithm algorithm) {
  switch (algorithm) {
    case SHA256:
      return std::make_unique<SecureHashSHA256>();
    case SHA512:
      return std::make_unique<SecureHashSHA512>();
    default:
      NOTIMPLEMENTED();
      return nullptr;
  }
}

void SecureHash::Update(const void* input, size_t len) {
  // SAFETY: This API is deprecated & being migrated away from. It can't be
  // safely implemented at the moment.
  // TODO(https://crbug.com/364687923): Remove this.
  return Update(UNSAFE_BUFFERS(
      base::span<const uint8_t>(static_cast<const uint8_t*>(input), len)));
}

void SecureHash::Finish(void* output, size_t len) {
  // SAFETY: This API is deprecated & being migrated away from. It can't be
  // safely implemented at the moment.
  // TODO(https://crbug.com/364687923): Remove this.
  return Finish(
      UNSAFE_BUFFERS(base::span<uint8_t>(static_cast<uint8_t*>(output), len)));
}

}  // namespace crypto
