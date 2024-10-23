// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hash.h"

#include "base/notreached.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::hash {

namespace {

const EVP_MD* EVPMDForHashKind(HashKind kind) {
  switch (kind) {
    case HashKind::kSha1:
      return EVP_sha1();
    case HashKind::kSha256:
      return EVP_sha256();
    case HashKind::kSha512:
      return EVP_sha512();
  }
  NOTREACHED();
}

}  // namespace

void Hash(HashKind kind,
          base::span<const uint8_t> data,
          base::span<uint8_t> digest) {
  const EVP_MD* md = EVPMDForHashKind(kind);
  CHECK_EQ(digest.size(), EVP_MD_size(md));

  CHECK(EVP_Digest(data.data(), data.size(), digest.data(), nullptr, md,
                   nullptr));
}

std::array<uint8_t, kSha1Size> Sha1(base::span<const uint8_t> data) {
  std::array<uint8_t, kSha1Size> result;
  Hash(HashKind::kSha1, data, result);
  return result;
}

std::array<uint8_t, kSha256Size> Sha256(base::span<const uint8_t> data) {
  std::array<uint8_t, kSha256Size> result;
  Hash(HashKind::kSha256, data, result);
  return result;
}

std::array<uint8_t, kSha512Size> Sha512(base::span<const uint8_t> data) {
  std::array<uint8_t, kSha512Size> result;
  Hash(HashKind::kSha512, data, result);
  return result;
}

Hasher::Hasher(HashKind kind) {
  CHECK(EVP_DigestInit(ctx_.get(), EVPMDForHashKind(kind)));
}

Hasher::Hasher(const Hasher& other) {
  *this = other;
}

Hasher::Hasher(Hasher&& other) {
  *this = other;
}

Hasher& Hasher::operator=(const Hasher& other) {
  CHECK(EVP_MD_CTX_copy_ex(ctx_.get(), other.ctx_.get()));
  return *this;
}

Hasher& Hasher::operator=(Hasher&& other) {
  ctx_ = std::move(other.ctx_);
  return *this;
}

Hasher::~Hasher() = default;

void Hasher::Update(base::span<const uint8_t> data) {
  CHECK(EVP_DigestUpdate(ctx_.get(), data.data(), data.size()));
}

void Hasher::Finish(base::span<uint8_t> digest) {
  CHECK_EQ(digest.size(), EVP_MD_CTX_size(ctx_.get()));
  CHECK(EVP_DigestFinal(ctx_.get(), digest.data(), nullptr));
}

}  // namespace crypto::hash
