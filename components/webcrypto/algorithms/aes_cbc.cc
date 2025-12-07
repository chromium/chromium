// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bits.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "components/webcrypto/algorithms/aes.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/cipher.h"

namespace webcrypto {

namespace {

const EVP_CIPHER* GetAESCipherByKeyLength(size_t key_length_bytes) {
  // 192-bit AES is intentionally unsupported (http://crbug.com/533699).
  switch (key_length_bytes) {
    case 16:
      return EVP_aes_128_cbc();
    case 32:
      return EVP_aes_256_cbc();
    default:
      return nullptr;
  }
}

Status AesCbcEncryptDecrypt(EncryptOrDecrypt cipher_operation,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            base::span<const uint8_t> data,
                            std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const blink::WebCryptoAesCbcParams* params = algorithm.AesCbcParams();
  const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(key);

  if (params->Iv().size() != 16)
    return Status::ErrorIncorrectSizeAesCbcIv();

  // Note: PKCS padding is enabled by default
  const EVP_CIPHER* const cipher = GetAESCipherByKeyLength(raw_key.size());
  DCHECK(cipher);

  bssl::ScopedEVP_CIPHER_CTX context;
  if (!EVP_CipherInit_ex(context.get(), cipher, nullptr, &raw_key[0],
                         params->Iv().data(), cipher_operation)) {
    return Status::OperationError();
  }

  if (cipher_operation == ENCRYPT) {
    // CBC encryption is padded by at least one byte, up to a block boundary.
    // (This cannot overflow because `data.size()` is at most `PTRDIFF_MAX`. If
    // it did wraparound, `EVP_CipherUpdate_ex` and `EVP_CipherFinal_ex2` will
    // check the provided bounds and cleanly fail.)
    buffer->resize(
        base::bits::AlignUp(data.size() + 1, size_t{AES_BLOCK_SIZE}));
  } else {
    // CBC decryption will output at most the input size.
    buffer->resize(data.size());
  }

  size_t output_len = 0;
  if (!EVP_CipherUpdate_ex(context.get(), buffer->data(), &output_len,
                           buffer->size(), data.data(), data.size())) {
    return Status::OperationError();
  }
  auto remainder = base::span(*buffer).subspan(output_len);
  size_t final_output_chunk_len = 0;
  if (!EVP_CipherFinal_ex2(context.get(), remainder.data(),
                           &final_output_chunk_len, remainder.size())) {
    return Status::OperationError();
  }

  buffer->resize(output_len + final_output_chunk_len);
  return Status::Success();
}

class AesCbcImplementation : public AesAlgorithm {
 public:
  AesCbcImplementation() : AesAlgorithm("CBC") {}

  Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override {
    return AesCbcEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override {
    return AesCbcEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateAesCbcImplementation() {
  return std::make_unique<AesCbcImplementation>();
}

}  // namespace webcrypto
