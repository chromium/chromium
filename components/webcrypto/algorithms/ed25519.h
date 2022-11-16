// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_ED25519_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_ED25519_H_

#include "components/webcrypto/algorithm_implementation.h"

namespace webcrypto {

// This class implements Ed25519, a signature scheme specified in RFC 8032.
// https://www.rfc-editor.org/rfc/rfc8032
class Ed25519Implementation : public AlgorithmImplementation {
 public:
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
              base::span<const uint8_t> data,
              std::vector<uint8_t>* buffer) const override;

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                base::span<const uint8_t> signature,
                base::span<const uint8_t> data,
                bool* signature_match) const override;

  Status DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                                blink::WebCryptoKeyType type,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                base::span<const uint8_t> key_data,
                                blink::WebCryptoKey* key) const override;

 private:
  Status ImportKeyRaw(base::span<const uint8_t> key_data,
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

  Status ExportKeyRaw(const blink::WebCryptoKey& key,
                      std::vector<uint8_t>* buffer) const;

  Status ExportKeyPkcs8(const blink::WebCryptoKey& key,
                        std::vector<uint8_t>* buffer) const;

  Status ExportKeySpki(const blink::WebCryptoKey& key,
                       std::vector<uint8_t>* buffer) const;

  Status ExportKeyJwk(const blink::WebCryptoKey& key,
                      std::vector<uint8_t>* buffer) const;

  const blink::WebCryptoKeyUsageMask all_public_key_usages_{
      blink::kWebCryptoKeyUsageVerify};
  const blink::WebCryptoKeyUsageMask all_private_key_usages_{
      blink::kWebCryptoKeyUsageSign};
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_ED25519_H_
