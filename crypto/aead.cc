// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aead.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto {

namespace {

const EVP_AEAD* AeadForAlgorithm(Aead::AeadAlgorithm algorithm) {
  switch (algorithm) {
    case Aead::AES_128_CTR_HMAC_SHA256:
      return EVP_aead_aes_128_ctr_hmac_sha256();
    case Aead::AES_256_GCM:
      return EVP_aead_aes_256_gcm();
    case Aead::AES_256_GCM_SIV:
      return EVP_aead_aes_256_gcm_siv();
    case Aead::CHACHA20_POLY1305:
      return EVP_aead_chacha20_poly1305();
  }
}

}  // namespace

namespace aead {

size_t KeySizeFor(Algorithm algorithm) {
  return EVP_AEAD_key_length(AeadForAlgorithm(algorithm));
}

size_t NonceSizeFor(Algorithm algorithm) {
  return EVP_AEAD_nonce_length(AeadForAlgorithm(algorithm));
}

std::vector<uint8_t> Seal(Algorithm algorithm,
                          base::span<const uint8_t> key,
                          base::span<const uint8_t> plaintext,
                          base::span<const uint8_t> nonce,
                          base::span<const uint8_t> associated_data) {
  Aead aead(algorithm, key);
  return aead.Seal(plaintext, nonce, associated_data);
}

std::optional<std::vector<uint8_t>> Open(
    Algorithm algorithm,
    base::span<const uint8_t> key,
    base::span<const uint8_t> ciphertext,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> associated_data) {
  Aead aead(algorithm, key);
  return aead.Open(ciphertext, nonce, associated_data);
}

}  // namespace aead

Aead::Aead(AeadAlgorithm algorithm) : algorithm_(algorithm) {}

Aead::Aead(AeadAlgorithm algorithm, base::span<const uint8_t> key)
    : Aead(algorithm) {
  Init(key);
}

Aead::~Aead() = default;

void Aead::Init(base::span<const uint8_t> key) {
  if (EVP_AEAD_CTX_init(ctx_.get(), AeadForAlgorithm(algorithm_), key.data(),
                        key.size(), EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr)) {
    initialized_ = true;
  }
}

void Aead::Init(const std::string* key) {
  Init(base::as_byte_span(*key));
}

std::vector<uint8_t> Aead::Seal(
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> additional_data) const {
  if (!initialized_) {
    return {};
  }
  const EVP_AEAD* aead = EVP_AEAD_CTX_aead(ctx_.get());
  size_t max_output_length =
      base::CheckAdd(plaintext.size(), EVP_AEAD_max_overhead(aead))
          .ValueOrDie();
  std::vector<uint8_t> ret(max_output_length);

  std::optional<size_t> output_length =
      Seal(plaintext, nonce, additional_data, ret);
  CHECK(output_length);
  ret.resize(*output_length);
  return ret;
}

bool Aead::Seal(std::string_view plaintext,
                std::string_view nonce,
                std::string_view additional_data,
                std::string* ciphertext) const {
  if (!initialized_) {
    ciphertext->clear();
    return false;
  }
  const EVP_AEAD* aead = EVP_AEAD_CTX_aead(ctx_.get());
  size_t max_output_length =
      base::CheckAdd(plaintext.size(), EVP_AEAD_max_overhead(aead))
          .ValueOrDie();
  ciphertext->resize(max_output_length);

  std::optional<size_t> output_length =
      Seal(base::as_byte_span(plaintext), base::as_byte_span(nonce),
           base::as_byte_span(additional_data),
           base::as_writable_byte_span(*ciphertext));
  if (!output_length) {
    ciphertext->clear();
    return false;
  }

  ciphertext->resize(*output_length);
  return true;
}

std::optional<std::vector<uint8_t>> Aead::Open(
    base::span<const uint8_t> ciphertext,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> additional_data) const {
  if (!initialized_) {
    return std::nullopt;
  }

  const size_t max_output_length = ciphertext.size();
  std::vector<uint8_t> ret(max_output_length);

  std::optional<size_t> output_length =
      Open(ciphertext, nonce, additional_data, ret);
  if (!output_length) {
    return std::nullopt;
  }

  ret.resize(*output_length);
  return ret;
}

bool Aead::Open(std::string_view ciphertext,
                std::string_view nonce,
                std::string_view additional_data,
                std::string* plaintext) const {
  if (!initialized_) {
    plaintext->clear();
    return false;
  }

  const size_t max_output_length = ciphertext.size();
  plaintext->resize(max_output_length);

  std::optional<size_t> output_length =
      Open(base::as_byte_span(ciphertext), base::as_byte_span(nonce),
           base::as_byte_span(additional_data),
           base::as_writable_byte_span(*plaintext));
  if (!output_length) {
    plaintext->clear();
    return false;
  }

  plaintext->resize(*output_length);
  return true;
}

size_t Aead::KeyLength() const {
  return aead::KeySizeFor(algorithm_);
}

size_t Aead::NonceLength() const {
  return aead::NonceSizeFor(algorithm_);
}

std::optional<size_t> Aead::Seal(base::span<const uint8_t> plaintext,
                                 base::span<const uint8_t> nonce,
                                 base::span<const uint8_t> additional_data,
                                 base::span<uint8_t> out) const {
  if (!initialized_) {
    return std::nullopt;
  }
  DCHECK_EQ(NonceLength(), nonce.size());

  size_t out_len;
  if (!EVP_AEAD_CTX_seal(ctx_.get(), out.data(), &out_len, out.size(),
                         nonce.data(), nonce.size(), plaintext.data(),
                         plaintext.size(), additional_data.data(),
                         additional_data.size())) {
    return std::nullopt;
  }

  DCHECK_LE(out_len, out.size());
  return out_len;
}

std::optional<size_t> Aead::Open(base::span<const uint8_t> ciphertext,
                                 base::span<const uint8_t> nonce,
                                 base::span<const uint8_t> additional_data,
                                 base::span<uint8_t> out) const {
  if (!initialized_) {
    return std::nullopt;
  }
  DCHECK_EQ(NonceLength(), nonce.size());
  bssl::ScopedEVP_AEAD_CTX ctx;

  size_t out_len;
  if (!EVP_AEAD_CTX_open(ctx_.get(), out.data(), &out_len, out.size(),
                         nonce.data(), nonce.size(), ciphertext.data(),
                         ciphertext.size(), additional_data.data(),
                         additional_data.size())) {
    return std::nullopt;
  }

  DCHECK_LE(out_len, out.size());
  return out_len;
}

}  // namespace crypto
