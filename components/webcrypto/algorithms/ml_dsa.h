// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_ML_DSA_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_ML_DSA_H_

#include "components/webcrypto/algorithm_implementation.h"

namespace webcrypto {

// Implementation of ML-DSA (44, 65, 87)
class MlDsaImplementation : public AlgorithmImplementation {
 public:
  MlDsaImplementation() = default;

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

  Status Sign(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              base::span<const uint8_t> message,
              std::vector<uint8_t>* signature) const override;

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                base::span<const uint8_t> signature,
                base::span<const uint8_t> message,
                bool* signature_match) const override;

  bool Supports(blink::WebCryptoOperation op,
                const blink::WebCryptoAlgorithm& algorithm,
                std::optional<unsigned int> length_bits) const override;

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
      blink::kWebCryptoKeyUsageVerify;
  const blink::WebCryptoKeyUsageMask all_private_key_usages_ =
      blink::kWebCryptoKeyUsageSign;
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_ML_DSA_H_
