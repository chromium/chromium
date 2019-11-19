// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aead.h"

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/strings/string_util.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto {

Aead::Aead(AeadAlgorithm algorithm) {
  EnsureOpenSSLInit();
  switch (algorithm) {
    case AES_128_CTR_HMAC_SHA256:
      aead_ = EVP_aead_aes_128_ctr_hmac_sha256();
      break;
    case AES_256_GCM:
      aead_ = EVP_aead_aes_256_gcm();
      break;
    case AES_256_GCM_SIV:
      aead_ = EVP_aead_aes_256_gcm_siv();
      break;
  }
}

Aead::~Aead() = default;

void Aead::Init(base::span<const uint8_t> key) {
  DCHECK(!key_);
  DCHECK_EQ(KeyLength(), key.size());
  key_ = key;
}

static base::span<const uint8_t> ToSpan(base::StringPiece sp) {
  return base::as_bytes(base::make_span(sp));
}

void Aead::Init(const std::string* key) {
  Init(ToSpan(*key));
}

std::vector<uint8_t> Aead::Seal(
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> additional_data) const {
  const size_t max_output_length =
      EVP_AEAD_max_overhead(aead_) + plaintext.size();
  CHECK(max_output_length >= plaintext.size());
  std::vector<uint8_t> ret;
  ret.resize(max_output_length);

  size_t output_length;
  CHECK(Seal(plaintext, nonce, additional_data, ret.data(), &output_length,
             max_output_length));
  ret.resize(output_length);
  return ret;
}

bool Aead::Seal(base::StringPiece plaintext,
                base::StringPiece nonce,
                base::StringPiece additional_data,
                std::string* ciphertext) const {
  const size_t max_output_length =
      EVP_AEAD_max_overhead(aead_) + plaintext.size();
  CHECK(max_output_length + 1 >= plaintext.size());
  uint8_t* out_ptr = reinterpret_cast<uint8_t*>(
      base::WriteInto(ciphertext, max_output_length + 1));

  size_t output_length;
  if (!Seal(ToSpan(plaintext), ToSpan(nonce), ToSpan(additional_data), out_ptr,
            &output_length, max_output_length)) {
    ciphertext->clear();
    return false;
  }

  ciphertext->resize(output_length);
  return true;
}

base::Optional<std::vector<uint8_t>> Aead::Open(
    base::span<const uint8_t> ciphertext,
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> additional_data) const {
  const size_t max_output_length = ciphertext.size();
  std::vector<uint8_t> ret;
  ret.resize(max_output_length);

  size_t output_length;
  if (!Open(ciphertext, nonce, additional_data, ret.data(), &output_length,
            max_output_length)) {
    return base::nullopt;
  }

  ret.resize(output_length);
  return ret;
}

bool Aead::Open(base::StringPiece ciphertext,
                base::StringPiece nonce,
                base::StringPiece additional_data,
                std::string* plaintext) const {
  const size_t max_output_length = ciphertext.size();
  CHECK(max_output_length + 1 > max_output_length);
  uint8_t* out_ptr = reinterpret_cast<uint8_t*>(
      base::WriteInto(plaintext, max_output_length + 1));

  size_t output_length;
  if (!Open(ToSpan(ciphertext), ToSpan(nonce), ToSpan(additional_data), out_ptr,
            &output_length, max_output_length)) {
    plaintext->clear();
    return false;
  }

  plaintext->resize(output_length);
  return true;
}

size_t Aead::KeyLength() const {
  return EVP_AEAD_key_length(aead_);
}

size_t Aead::NonceLength() const {
  return EVP_AEAD_nonce_length(aead_);
}

bool Aead::Seal(base::span<const uint8_t> plaintext,
                base::span<const uint8_t> nonce,
                base::span<const uint8_t> additional_data,
                uint8_t* out,
                size_t* output_length,
                size_t max_output_length) const {
  DCHECK(key_);
  DCHECK_EQ(NonceLength(), nonce.size());
  bssl::ScopedEVP_AEAD_CTX ctx;

  if (!EVP_AEAD_CTX_init(ctx.get(), aead_, key_->data(), key_->size(),
                         EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr) ||
      !EVP_AEAD_CTX_seal(ctx.get(), out, output_length, max_output_length,
                         nonce.data(), nonce.size(), plaintext.data(),
                         plaintext.size(), additional_data.data(),
                         additional_data.size())) {
    return false;
  }

  DCHECK_LE(*output_length, max_output_length);
  return true;
}

bool Aead::Open(base::span<const uint8_t> plaintext,
                base::span<const uint8_t> nonce,
                base::span<const uint8_t> additional_data,
                uint8_t* out,
                size_t* output_length,
                size_t max_output_length) const {
  DCHECK(key_);
  DCHECK_EQ(NonceLength(), nonce.size());
  bssl::ScopedEVP_AEAD_CTX ctx;

  if (!EVP_AEAD_CTX_init(ctx.get(), aead_, key_->data(), key_->size(),
                         EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr) ||
      !EVP_AEAD_CTX_open(ctx.get(), out, output_length, max_output_length,
                         nonce.data(), nonce.size(), plaintext.data(),
                         plaintext.size(), additional_data.data(),
                         additional_data.size())) {
    return false;
  }

  DCHECK_LE(*output_length, max_output_length);
  return true;
}

}  // namespace crypto
