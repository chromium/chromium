// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/obsolete/sha1.h"

#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::obsolete {

// static
std::array<uint8_t, kSha1Size> Sha1::Hash(std::string_view data) {
  return Hash(base::as_byte_span(data));
}

// static
std::array<uint8_t, kSha1Size> Sha1::Hash(base::span<const uint8_t> data) {
  std::array<uint8_t, kSha1Size> result;
  Sha1 hasher;
  hasher.Update(data);
  hasher.Finish(result);
  return result;
}

Sha1::Sha1() {
  CHECK(EVP_DigestInit(ctx_.get(), EVP_sha1()));
}

Sha1::Sha1(const Sha1& other) {
  *this = other;
}

Sha1::Sha1(Sha1&& other) {
  *this = other;
}

Sha1& Sha1::operator=(const Sha1& other) {
  CHECK(EVP_MD_CTX_copy_ex(ctx_.get(), other.ctx_.get()));
  return *this;
}

Sha1& Sha1::operator=(Sha1&& other) {
  ctx_ = std::move(other.ctx_);
  return *this;
}

Sha1::~Sha1() = default;

// static
Sha1 Sha1::MakeSha1HasherForTesting() {
  return {};
}

// static
std::array<uint8_t, Sha1::kSize> Sha1::HashForTesting(
    base::span<const uint8_t> data) {
  return Hash(data);
}

void Sha1::Update(std::string_view data) {
  Update(base::as_byte_span(data));
}

void Sha1::Update(base::span<const uint8_t> data) {
  CHECK(EVP_DigestUpdate(ctx_.get(), data.data(), data.size()));
}

void Sha1::Finish(base::span<uint8_t, Sha1::kSize> result) {
  CHECK(EVP_DigestFinal(ctx_.get(), result.data(), nullptr));
}

std::array<uint8_t, Sha1::kSize> Sha1::Finish() {
  std::array<uint8_t, Sha1::kSize> result;
  Finish(result);
  return result;
}

}  // namespace crypto::obsolete
