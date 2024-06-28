// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/aes_cbc_crypto.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "crypto/openssl_util.h"
#include "crypto/symmetric_key.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

// Notes on using OpenSSL:
// https://www.openssl.org/docs/man1.1.0/crypto/EVP_DecryptUpdate.html
// The documentation for EVP_DecryptUpdate() only states
//   "EVP_DecryptInit_ex(), EVP_DecryptUpdate() and EVP_DecryptFinal_ex()
//   are the corresponding decryption operations. EVP_DecryptFinal() will
//   return an error code if padding is enabled and the final block is not
//   correctly formatted. The parameters and restrictions are identical
//   to the encryption operations except that if padding is enabled ..."
// As this implementation does not use padding, the last part should not be
// an issue. However, there is no mention whether data can be decrypted
// block-by-block or if all the data must be unencrypted at once.
//
// The documentation for EVP_EncryptUpdate() (same page as above) states
//   "EVP_EncryptUpdate() encrypts inl bytes from the buffer in and writes
//   the encrypted version to out. This function can be called multiple times
//   to encrypt successive blocks of data."
// Given that the EVP_Decrypt* methods have the same restrictions, the code
// below assumes that EVP_DecryptUpdate() can be called on a block-by-block
// basis. A test in aes_cbc_crypto_unittest.cc verifies this.

namespace media {

AesCbcCrypto::AesCbcCrypto() = default;
AesCbcCrypto::~AesCbcCrypto() = default;

bool AesCbcCrypto::Initialize(const crypto::SymmetricKey& key,
                              base::span<const uint8_t> iv) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // This uses AES-CBC-128, so the key must be 128 bits.
  const EVP_CIPHER* cipher = EVP_aes_128_cbc();
  const uint8_t* key_data = reinterpret_cast<const uint8_t*>(key.key().data());
  if (key.key().length() != EVP_CIPHER_key_length(cipher)) {
    DVLOG(1) << "Key length is incorrect.";
    return false;
  }

  // |iv| must also be 128 bits.
  if (iv.size_bytes() != EVP_CIPHER_iv_length(cipher)) {
    DVLOG(1) << "IV length is incorrect.";
    return false;
  }

  if (!EVP_DecryptInit_ex(ctx_.get(), cipher, nullptr, key_data, iv.data())) {
    DVLOG(1) << "EVP_DecryptInit_ex() failed.";
    return false;
  }

  if (!EVP_CIPHER_CTX_set_padding(ctx_.get(), 0)) {
    DVLOG(1) << "EVP_CIPHER_CTX_set_padding() failed.";
    return false;
  }

  return true;
}

bool AesCbcCrypto::Decrypt(base::span<const uint8_t> encrypted_data,
                           uint8_t* decrypted_data) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (encrypted_data.size_bytes() % EVP_CIPHER_CTX_block_size(ctx_.get()) !=
      0) {
    DVLOG(1) << "Encrypted bytes not a multiple of block size.";
    return false;
  }

  int out_length;
  if (!EVP_DecryptUpdate(ctx_.get(), decrypted_data, &out_length,
                         encrypted_data.data(), encrypted_data.size_bytes())) {
    DVLOG(1) << "EVP_DecryptUpdate() failed.";
    return false;
  }

  return encrypted_data.size_bytes() == base::checked_cast<size_t>(out_length);
}

}  // namespace media
