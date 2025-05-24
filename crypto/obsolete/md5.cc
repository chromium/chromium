// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/obsolete/md5.h"

#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::obsolete {

// static
std::array<uint8_t, Md5::kSize> Md5::Hash(std::string_view data) {
  return Hash(base::as_byte_span(data));
}

// static
std::array<uint8_t, Md5::kSize> Md5::Hash(base::span<const uint8_t> data) {
  std::array<uint8_t, Md5::kSize> result;
  Md5 hasher;
  hasher.Update(data);
  hasher.Finish(result);
  return result;
}

Md5::Md5() {
  CHECK(EVP_DigestInit(ctx_.get(), EVP_md5()));
}

Md5::Md5(const Md5& other) {
  *this = other;
}

Md5::Md5(Md5&& other) {
  *this = other;
}

Md5& Md5::operator=(const Md5& other) {
  CHECK(EVP_MD_CTX_copy_ex(ctx_.get(), other.ctx_.get()));
  return *this;
}

Md5& Md5::operator=(Md5&& other) {
  ctx_ = std::move(other.ctx_);
  return *this;
}

Md5::~Md5() = default;

Md5 Md5::MakeMd5HasherForTesting() {
  return {};
}

std::array<uint8_t, Md5::kSize> Md5::HashForTesting(
    base::span<const uint8_t> data) {
  return Hash(data);
}

void Md5::Update(std::string_view data) {
  Update(base::as_byte_span(data));
}

void Md5::Update(base::span<const uint8_t> data) {
  CHECK(EVP_DigestUpdate(ctx_.get(), data.data(), data.size()));
}

void Md5::Finish(base::span<uint8_t, Md5::kSize> result) {
  CHECK(EVP_DigestFinal(ctx_.get(), result.data(), nullptr));
}

std::array<uint8_t, Md5::kSize> Md5::Finish() {
  std::array<uint8_t, Md5::kSize> result;
  Finish(result);
  return result;
}

}  // namespace crypto::obsolete
