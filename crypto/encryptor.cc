// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "crypto/encryptor.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/openssl_util.h"
#include "crypto/symmetric_key.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto {

namespace {

const EVP_CIPHER* GetCipherForKey(const SymmetricKey* key) {
  switch (key->key().length()) {
    case 16: return EVP_aes_128_cbc();
    case 32: return EVP_aes_256_cbc();
    default:
      return nullptr;
  }
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////
// Encryptor Implementation.

Encryptor::Encryptor() : key_(nullptr), mode_(CBC) {}

Encryptor::~Encryptor() = default;

bool Encryptor::Init(const SymmetricKey* key, Mode mode, std::string_view iv) {
  return Init(key, mode, base::as_bytes(base::make_span(iv)));
}

bool Encryptor::Init(const SymmetricKey* key,
                     Mode mode,
                     base::span<const uint8_t> iv) {
  DCHECK(key);
  DCHECK(mode == CBC || mode == CTR);

  if (mode == CBC && iv.size() != AES_BLOCK_SIZE)
    return false;
  // CTR mode passes the starting counter separately, via SetCounter().
  if (mode == CTR && !iv.empty())
    return false;

  if (GetCipherForKey(key) == nullptr)
    return false;

  key_ = key;
  mode_ = mode;
  iv_.assign(iv.begin(), iv.end());
  return true;
}

bool Encryptor::Encrypt(std::string_view plaintext, std::string* ciphertext) {
  return CryptString(/*do_encrypt=*/true, plaintext, ciphertext);
}

bool Encryptor::Encrypt(base::span<const uint8_t> plaintext,
                        std::vector<uint8_t>* ciphertext) {
  return CryptBytes(/*do_encrypt=*/true, plaintext, ciphertext);
}

bool Encryptor::Decrypt(std::string_view ciphertext, std::string* plaintext) {
  return CryptString(/*do_encrypt=*/false, ciphertext, plaintext);
}

bool Encryptor::Decrypt(base::span<const uint8_t> ciphertext,
                        std::vector<uint8_t>* plaintext) {
  return CryptBytes(/*do_encrypt=*/false, ciphertext, plaintext);
}

bool Encryptor::SetCounter(std::string_view counter) {
  return SetCounter(base::as_bytes(base::make_span(counter)));
}

bool Encryptor::SetCounter(base::span<const uint8_t> counter) {
  if (mode_ != CTR)
    return false;
  if (counter.size() != 16u)
    return false;

  iv_.assign(counter.begin(), counter.end());
  return true;
}

bool Encryptor::CryptString(bool do_encrypt,
                            std::string_view input,
                            std::string* output) {
  std::string result(MaxOutput(do_encrypt, input.size()), '\0');
  std::optional<size_t> len =
      (mode_ == CTR)
          ? CryptCTR(do_encrypt, base::as_bytes(base::make_span(input)),
                     base::as_writable_bytes(base::make_span(result)))
          : Crypt(do_encrypt, base::as_bytes(base::make_span(input)),
                  base::as_writable_bytes(base::make_span(result)));
  if (!len)
    return false;

  result.resize(*len);
  *output = std::move(result);
  return true;
}

bool Encryptor::CryptBytes(bool do_encrypt,
                           base::span<const uint8_t> input,
                           std::vector<uint8_t>* output) {
  std::vector<uint8_t> result(MaxOutput(do_encrypt, input.size()));
  std::optional<size_t> len = (mode_ == CTR)
                                  ? CryptCTR(do_encrypt, input, result)
                                  : Crypt(do_encrypt, input, result);
  if (!len)
    return false;

  result.resize(*len);
  *output = std::move(result);
  return true;
}

size_t Encryptor::MaxOutput(bool do_encrypt, size_t length) {
  size_t result = length + ((do_encrypt && mode_ == CBC) ? 16 : 0);
  CHECK_GE(result, length);  // Overflow
  return result;
}

std::optional<size_t> Encryptor::Crypt(bool do_encrypt,
                                       base::span<const uint8_t> input,
                                       base::span<uint8_t> output) {
  DCHECK(key_);  // Must call Init() before En/De-crypt.

  const EVP_CIPHER* cipher = GetCipherForKey(key_);
  DCHECK(cipher);  // Already handled in Init();

  const std::string& key = key_->key();
  DCHECK_EQ(EVP_CIPHER_iv_length(cipher), iv_.size());
  DCHECK_EQ(EVP_CIPHER_key_length(cipher), key.size());

  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_CIPHER_CTX ctx;
  if (!EVP_CipherInit_ex(ctx.get(), cipher, nullptr,
                         reinterpret_cast<const uint8_t*>(key.data()),
                         iv_.data(), do_encrypt)) {
    return std::nullopt;
  }

  // Encrypting needs a block size of space to allow for any padding.
  CHECK_GE(output.size(), input.size() + (do_encrypt ? iv_.size() : 0));
  int out_len;
  if (!EVP_CipherUpdate(ctx.get(), output.data(), &out_len, input.data(),
                        input.size()))
    return std::nullopt;

  // Write out the final block plus padding (if any) to the end of the data
  // just written.
  int tail_len;
  if (!EVP_CipherFinal_ex(ctx.get(), output.data() + out_len, &tail_len))
    return std::nullopt;

  out_len += tail_len;
  DCHECK_LE(out_len, static_cast<int>(output.size()));
  return out_len;
}

std::optional<size_t> Encryptor::CryptCTR(bool do_encrypt,
                                          base::span<const uint8_t> input,
                                          base::span<uint8_t> output) {
  if (iv_.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Counter value not set in CTR mode.";
    return std::nullopt;
  }

  AES_KEY aes_key;
  if (AES_set_encrypt_key(reinterpret_cast<const uint8_t*>(key_->key().data()),
                          key_->key().size() * 8, &aes_key) != 0) {
    return std::nullopt;
  }

  uint8_t ecount_buf[AES_BLOCK_SIZE] = { 0 };
  unsigned int block_offset = 0;

  // |output| must have room for |input|.
  CHECK_GE(output.size(), input.size());
  // Note AES_ctr128_encrypt() will update |iv_|. However, this method discards
  // |ecount_buf| and |block_offset|, so this is not quite a streaming API.
  AES_ctr128_encrypt(input.data(), output.data(), input.size(), &aes_key,
                     iv_.data(), ecount_buf, &block_offset);
  return input.size();
}

}  // namespace crypto
