// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "components/webcrypto/algorithms/aes.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/boringssl/src/include/openssl/aead.h"

namespace webcrypto {

namespace {

const EVP_AEAD* GetAesGcmAlgorithmFromKeySize(size_t key_size_bytes) {
  switch (key_size_bytes) {
    case 16:
      return EVP_aead_aes_128_gcm();
    case 32:
      return EVP_aead_aes_256_gcm();
    default:
      return nullptr;
  }
}

Status AesGcmEncryptDecrypt(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            base::span<const uint8_t> data,
                            std::vector<uint8_t>* buffer) {
  const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(key);
  const blink::WebCryptoAesGcmParams* params = algorithm.AesGcmParams();

  // The WebCrypto spec defines the default value for the tag length, as well as
  // the allowed values for tag length.
  unsigned int tag_length_bits = 128;
  if (params->HasTagLengthBits()) {
    tag_length_bits = params->OptionalTagLengthBits();
    if (tag_length_bits != 32 && tag_length_bits != 64 &&
        tag_length_bits != 96 && tag_length_bits != 104 &&
        tag_length_bits != 112 && tag_length_bits != 120 &&
        tag_length_bits != 128) {
      return Status::ErrorInvalidAesGcmTagLength();
    }
  }

  return AeadEncryptDecrypt(mode, raw_key, data, tag_length_bits / 8,
                            params->Iv(), params->OptionalAdditionalData(),
                            GetAesGcmAlgorithmFromKeySize(raw_key.size()),
                            buffer);
}

class AesGcmImplementation : public AesAlgorithm {
 public:
  AesGcmImplementation() : AesAlgorithm("GCM") {}

  Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override {
    return AesGcmEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override {
    return AesGcmEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateAesGcmImplementation() {
  return std::make_unique<AesGcmImplementation>();
}

}  // namespace webcrypto
