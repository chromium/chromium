
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "components/webcrypto/algorithms/aes.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
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
                            const CryptoData& data,
                            std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const blink::WebCryptoAesCbcParams* params = algorithm.AesCbcParams();
  const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(key);

  if (params->Iv().size() != 16)
    return Status::ErrorIncorrectSizeAesCbcIv();

  // According to the openssl docs, the amount of data written may be as large
  // as (data_size + cipher_block_size - 1), constrained to a multiple of
  // cipher_block_size.
  base::CheckedNumeric<int> output_max_len = data.byte_length();
  output_max_len += AES_BLOCK_SIZE - 1;
  if (!output_max_len.IsValid())
    return Status::ErrorDataTooLarge();

  const unsigned remainder =
      base::ValueOrDieForType<unsigned>(output_max_len % AES_BLOCK_SIZE);
  if (remainder != 0)
    output_max_len += AES_BLOCK_SIZE - remainder;
  if (!output_max_len.IsValid())
    return Status::ErrorDataTooLarge();

  // Note: PKCS padding is enabled by default
  const EVP_CIPHER* const cipher = GetAESCipherByKeyLength(raw_key.size());
  DCHECK(cipher);

  bssl::ScopedEVP_CIPHER_CTX context;
  if (!EVP_CipherInit_ex(context.get(), cipher, nullptr, &raw_key[0],
                         params->Iv().Data(), cipher_operation)) {
    return Status::OperationError();
  }

  buffer->resize(base::ValueOrDieForType<size_t>(output_max_len));

  int output_len = 0;
  if (!EVP_CipherUpdate(context.get(), buffer->data(), &output_len,
                        data.bytes(), data.byte_length())) {
    return Status::OperationError();
  }
  int final_output_chunk_len = 0;
  if (!EVP_CipherFinal_ex(context.get(), buffer->data() + output_len,
                          &final_output_chunk_len)) {
    return Status::OperationError();
  }

  const unsigned int final_output_len =
      static_cast<unsigned int>(output_len) +
      static_cast<unsigned int>(final_output_chunk_len);

  buffer->resize(final_output_len);

  return Status::Success();
}

class AesCbcImplementation : public AesAlgorithm {
 public:
  AesCbcImplementation() : AesAlgorithm("CBC") {}

  Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 const CryptoData& data,
                 std::vector<uint8_t>* buffer) const override {
    return AesCbcEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 const CryptoData& data,
                 std::vector<uint8_t>* buffer) const override {
    return AesCbcEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateAesCbcImplementation() {
  return std::make_unique<AesCbcImplementation>();
}

}  // namespace webcrypto
