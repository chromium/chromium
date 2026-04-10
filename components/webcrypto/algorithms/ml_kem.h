// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_ML_KEM_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_ML_KEM_H_

#include "components/webcrypto/algorithm_implementation.h"

namespace webcrypto {

// Implementation of ML-KEM (768, 1024)
class MlKemImplementation : public AlgorithmImplementation {
 public:
  MlKemImplementation() = default;

  Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usages,
                     GenerateKeyResult* result) const override;

  Status ImportKey(blink::WebCryptoKeyFormat format,
                   base::span<const uint8_t> key_data,
                   const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   blink::WebCryptoKey* key) const override;

  Status ExportKey(blink::WebCryptoKeyFormat format,
                   const blink::WebCryptoKey& key,
                   std::vector<uint8_t>* buffer) const override;

  Status Encapsulate(const blink::WebCryptoAlgorithm& algorithm,
                     const blink::WebCryptoKey& encapsulation_key,
                     std::vector<uint8_t>* out_shared_secret,
                     std::vector<uint8_t>* out_ciphertext) const override;

  Status Decapsulate(const blink::WebCryptoAlgorithm& algorithm,
                     const blink::WebCryptoKey& decapsulation_key,
                     base::span<const uint8_t> ciphertext,
                     std::vector<uint8_t>* out_shared_secret) const override;

  Status DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                                blink::WebCryptoKeyType type,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                base::span<const uint8_t> key_data,
                                blink::WebCryptoKey* key) const override;

 private:
  Status ImportKeyRawPublic(base::span<const uint8_t> key_data,
                            const blink::WebCryptoAlgorithm& algorithm,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usages,
                            blink::WebCryptoKey* key) const;

  Status ImportKeyRawSeed(base::span<const uint8_t> key_data,
                          const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usages,
                          blink::WebCryptoKey* key) const;

  Status ImportKeyPkcs8(base::span<const uint8_t> key_data,
                        const blink::WebCryptoAlgorithm& algorithm,
                        bool extractable,
                        blink::WebCryptoKeyUsageMask usages,
                        blink::WebCryptoKey* key) const;

  Status ImportKeySpki(base::span<const uint8_t> key_data,
                       const blink::WebCryptoAlgorithm& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usages,
                       blink::WebCryptoKey* key) const;

  Status ImportKeyJwk(base::span<const uint8_t> key_data,
                      const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      blink::WebCryptoKey* key) const;

  Status ExportKeyRawPublic(const blink::WebCryptoKey& key,
                            std::vector<uint8_t>* buffer) const;
  Status ExportKeyRawSeed(const blink::WebCryptoKey& key,
                          std::vector<uint8_t>* buffer) const;

  Status ExportKeyPkcs8(const blink::WebCryptoKey& key,
                        std::vector<uint8_t>* buffer) const;

  Status ExportKeySpki(const blink::WebCryptoKey& key,
                       std::vector<uint8_t>* buffer) const;

  Status ExportKeyJwk(const blink::WebCryptoKey& key,
                      std::vector<uint8_t>* buffer) const;

  const blink::WebCryptoKeyUsageMask all_public_key_usages_ =
      blink::kWebCryptoKeyUsageEncapsulateKey |
      blink::kWebCryptoKeyUsageEncapsulateBits;
  const blink::WebCryptoKeyUsageMask all_private_key_usages_ =
      blink::kWebCryptoKeyUsageDecapsulateKey |
      blink::kWebCryptoKeyUsageDecapsulateBits;
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_ML_KEM_H_
