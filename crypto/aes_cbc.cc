// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aes_cbc.h"

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

  int out_len;
  CHECK(EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &out_len,
                          plaintext.data(), plaintext.size()));

  int tail_len;
  CHECK(EVP_EncryptFinal_ex(ctx.get(),
                            // SAFETY: boringssl guarantees out_len is still
                            // inside ciphertext.
                            UNSAFE_BUFFERS(ciphertext.data() + out_len),
                            &tail_len));
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

  int out_len;
  CHECK(EVP_DecryptUpdate(ctx.get(), plaintext.data(), &out_len,
                          ciphertext.data(), ciphertext.size()));

  int tail_len;
  if (!EVP_DecryptFinal_ex(ctx.get(),
                           // SAFETY: boringssl guarantees out_len is still
                           // inside ciphertext.
                           UNSAFE_BUFFERS(plaintext.data() + out_len),
                           &tail_len)) {
    return std::nullopt;
  }

  plaintext.resize(out_len + tail_len);
  return plaintext;
}

}  // namespace crypto::aes_cbc
