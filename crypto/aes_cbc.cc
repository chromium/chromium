// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aes_cbc.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::aes_cbc {

std::vector<uint8_t> Encrypt(base::span<const uint8_t> key,
                             base::span<const uint8_t, kBlockSize> iv,
                             base::span<const uint8_t> plaintext) {
  const EVP_CIPHER* cipher =
      key.size() == 32 ? EVP_aes_256_cbc() : EVP_aes_128_cbc();

  CHECK_EQ(EVP_CIPHER_key_length(cipher), key.size());

  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_CIPHER_CTX ctx;
  CHECK(EVP_EncryptInit_ex(ctx.get(), cipher, nullptr, key.data(), iv.data()));

  std::vector<uint8_t> ciphertext(plaintext.size() + kBlockSize);

  size_t out_len;
  CHECK(EVP_EncryptUpdate_ex(ctx.get(), ciphertext.data(), &out_len,
                             ciphertext.size(), plaintext.data(),
                             plaintext.size()));

  auto remainder = base::span(ciphertext).subspan(out_len);
  size_t tail_len;
  CHECK(EVP_EncryptFinal_ex2(ctx.get(), remainder.data(), &tail_len,
                             remainder.size()));
  ciphertext.resize(out_len + tail_len);
  return ciphertext;
}

std::optional<std::vector<uint8_t>> Decrypt(
    base::span<const uint8_t> key,
    base::span<const uint8_t, kBlockSize> iv,
    base::span<const uint8_t> ciphertext) {
  const EVP_CIPHER* cipher =
      key.size() == 32 ? EVP_aes_256_cbc() : EVP_aes_128_cbc();
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_CIPHER_CTX ctx;
  CHECK(EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, key.data(), iv.data()));

  std::vector<uint8_t> plaintext(ciphertext.size());

  size_t out_len;
  CHECK(EVP_DecryptUpdate_ex(ctx.get(), plaintext.data(), &out_len,
                             plaintext.size(), ciphertext.data(),
                             ciphertext.size()));

  auto remainder = base::span(plaintext).subspan(out_len);
  size_t tail_len;
  if (!EVP_DecryptFinal_ex2(ctx.get(), remainder.data(), &tail_len,
                            remainder.size())) {
    return std::nullopt;
  }

  plaintext.resize(out_len + tail_len);
  return plaintext;
}

}  // namespace crypto::aes_cbc
