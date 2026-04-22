// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/encrypt.h"

#include "base/check.h"
#include "base/check_op.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace crypto::encrypt {

std::vector<uint8_t> Encrypt(EncryptionKind kind,
                             const crypto::keypair::PublicKey& key,
                             base::span<const uint8_t> plaintext) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CHECK_EQ(kind, RSA_OAEP_SHA1);

  // TODO(https://crbug.com/505090843): get rid of all the const_cast<> stuff
  // once the bssl API doesn't require it.
  EVP_PKEY* pkey = const_cast<EVP_PKEY*>(key.key());
  bssl::UniquePtr<EVP_PKEY_CTX> ctx(EVP_PKEY_CTX_new(pkey, nullptr));
  CHECK(ctx);
  CHECK(EVP_PKEY_encrypt_init(ctx.get()));
  CHECK(EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING));
  CHECK(EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), EVP_sha1()));

  size_t outlen = 0;
  CHECK(EVP_PKEY_encrypt(ctx.get(), nullptr, &outlen, plaintext.data(),
                         plaintext.size()));

  std::vector<uint8_t> ciphertext(outlen);
  CHECK(EVP_PKEY_encrypt(ctx.get(), ciphertext.data(), &outlen,
                         plaintext.data(), plaintext.size()));

  ciphertext.resize(outlen);
  return ciphertext;
}

std::optional<std::vector<uint8_t>> Decrypt(
    EncryptionKind kind,
    const crypto::keypair::PrivateKey& key,
    base::span<const uint8_t> ciphertext) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (kind != RSA_OAEP_SHA1) {
    return std::nullopt;
  }

  EVP_PKEY* pkey = const_cast<EVP_PKEY*>(key.key());
  bssl::UniquePtr<EVP_PKEY_CTX> ctx(EVP_PKEY_CTX_new(pkey, nullptr));

  CHECK(ctx);
  CHECK(EVP_PKEY_decrypt_init(ctx.get()));
  CHECK(EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING));
  CHECK(EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), EVP_sha1()));

  size_t outlen = 0;
  if (!EVP_PKEY_decrypt(ctx.get(), nullptr, &outlen, ciphertext.data(),
                        ciphertext.size())) {
    return std::nullopt;
  }

  std::vector<uint8_t> plaintext(outlen);
  if (!EVP_PKEY_decrypt(ctx.get(), plaintext.data(), &outlen, ciphertext.data(),
                        ciphertext.size())) {
    return std::nullopt;
  }

  plaintext.resize(outlen);
  return plaintext;
}

size_t GetCiphertextSize(const crypto::keypair::PublicKey& key) {
  return EVP_PKEY_size(const_cast<EVP_PKEY*>(key.key()));
}

size_t GetCiphertextSize(const crypto::keypair::PrivateKey& key) {
  return EVP_PKEY_size(const_cast<EVP_PKEY*>(key.key()));
}

size_t GetMaxPlaintextSize(EncryptionKind kind,
                           const crypto::keypair::PublicKey& key) {
  CHECK_EQ(kind, RSA_OAEP_SHA1);
  return GetCiphertextSize(key) - 2 - 2 * SHA_DIGEST_LENGTH;
}

size_t GetMaxPlaintextSize(EncryptionKind kind,
                           const crypto::keypair::PrivateKey& key) {
  CHECK_EQ(kind, RSA_OAEP_SHA1);
  return GetCiphertextSize(key) - 2 - 2 * SHA_DIGEST_LENGTH;
}

}  // namespace crypto::encrypt
