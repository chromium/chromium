// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_AES_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_AES_H_

#include <stdint.h>

#include <string_view>

#include "components/webcrypto/algorithm_implementation.h"

namespace webcrypto {

// Base class for AES algorithms that provides the implementation for key
// creation and export.
class AesAlgorithm : public AlgorithmImplementation {
 public:
  // |all_key_usages| is the set of all WebCrypto key usages that are
  // allowed for imported or generated keys. |jwk_suffix| is the suffix
  // used when constructing JWK names for the algorithm. For instance A128CBC
  // is the JWK name for 128-bit AES-CBC. The |jwk_suffix| in this case would
  // be "CBC".
  AesAlgorithm(blink::WebCryptoKeyUsageMask all_key_usages,
               std::string_view jwk_suffix);

  // This is the same as the other AesAlgorithm constructor where
  // |all_key_usages| is pre-filled to values for encryption/decryption
  // algorithms (supports usages for: encrypt, decrypt, wrap, unwrap).
  explicit AesAlgorithm(std::string_view jwk_suffix);

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

  Status DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                                blink::WebCryptoKeyType type,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                base::span<const uint8_t> key_data,
                                blink::WebCryptoKey* key) const override;

  Status GetKeyLength(const blink::WebCryptoAlgorithm& key_length_algorithm,
                      std::optional<unsigned int>* length_bits) const override;

 private:
  Status ImportKeyRaw(base::span<const uint8_t> key_data,
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

  Status ExportKeyJwk(const blink::WebCryptoKey& key,
                      std::vector<uint8_t>* buffer) const;

  const blink::WebCryptoKeyUsageMask all_key_usages_;
  const std::string jwk_suffix_;
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_AES_H_
