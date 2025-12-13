// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hash.h"

#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/memory/page_size.h"
#include "base/notreached.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::hash {

void Hash(HashKind kind,
          base::span<const uint8_t> data,
          base::span<uint8_t> digest) {
  const EVP_MD* md = EVPMDForHashKind(kind);
  CHECK_EQ(digest.size(), EVP_MD_size(md));

  CHECK(EVP_Digest(data.data(), data.size(), digest.data(), nullptr, md,
                   nullptr));
}

void Hash(HashKind kind, std::string_view data, base::span<uint8_t> digest) {
  Hash(kind, base::as_byte_span(data), digest);
}

std::array<uint8_t, kSha1Size> Sha1(base::span<const uint8_t> data) {
  std::array<uint8_t, kSha1Size> result;
  Hash(HashKind::kSha1, data, result);
  return result;
}

std::array<uint8_t, kSha1Size> Sha1(std::string_view data) {
  return Sha1(base::as_byte_span(data));
}

std::array<uint8_t, kSha256Size> Sha256(base::span<const uint8_t> data) {
  std::array<uint8_t, kSha256Size> result;
  Hash(HashKind::kSha256, data, result);
  return result;
}

std::array<uint8_t, kSha256Size> Sha256(std::string_view data) {
  return Sha256(base::as_byte_span(data));
}

std::array<uint8_t, kSha512Size> Sha512(base::span<const uint8_t> data) {
  std::array<uint8_t, kSha512Size> result;
  Hash(HashKind::kSha512, data, result);
  return result;
}

std::array<uint8_t, kSha512Size> Sha512(std::string_view data) {
  return Sha512(base::as_byte_span(data));
}

const EVP_MD* EVPMDForHashKind(HashKind kind) {
  switch (kind) {
    case HashKind::kSha1:
      return EVP_sha1();
    case HashKind::kSha256:
      return EVP_sha256();
    case HashKind::kSha384:
      return EVP_sha384();
    case HashKind::kSha512:
      return EVP_sha512();
  }
  NOTREACHED();
}

std::optional<HashKind> HashKindForEVPMD(const EVP_MD* evp_md) {
  switch (EVP_MD_type(evp_md)) {
    case NID_sha1:
      return crypto::hash::kSha1;
    case NID_sha256:
      return crypto::hash::kSha256;
    case NID_sha384:
      return crypto::hash::kSha384;
    case NID_sha512:
      return crypto::hash::kSha512;
  }
  return std::nullopt;
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
  CHECK(EVP_MD_CTX_md(ctx_.get()))
      << "Hasher::Update() called after Hasher::Finish()";
  CHECK(EVP_DigestUpdate(ctx_.get(), data.data(), data.size()));
}

void Hasher::Update(std::string_view data) {
  Update(base::as_byte_span(data));
}

void Hasher::Finish(base::span<uint8_t> digest) {
  CHECK(EVP_MD_CTX_md(ctx_.get())) << "Hasher::Finish() called multiple times";
  CHECK_EQ(digest.size(), EVP_MD_CTX_size(ctx_.get()));
  CHECK(EVP_DigestFinal(ctx_.get(), digest.data(), nullptr));
}

bool HashFile(HashKind kind, base::File* file, base::span<uint8_t> digest) {
  if (!file->IsValid()) {
    // Zero the out digest so that callers that fail to check the return value
    // won't read uninitialized values.
    std::ranges::fill(digest, 0);
    return false;
  }

  Hasher hasher(kind);

  while (true) {
    std::array<uint8_t, 4096> buffer;
    std::optional<size_t> bytes_read = file->ReadAtCurrentPos(buffer);
    if (!bytes_read.has_value()) {
      std::ranges::fill(digest, 0);
      return false;
    }
    if (bytes_read.value() == 0) {
      hasher.Finish(digest);
      return true;
    }
    hasher.Update(base::span(buffer).first(*bytes_read));
  }
}

}  // namespace crypto::hash
