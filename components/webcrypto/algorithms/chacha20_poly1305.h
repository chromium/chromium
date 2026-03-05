// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_CHACHA20_POLY1305_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_CHACHA20_POLY1305_H_

#include "components/webcrypto/algorithm_implementation.h"

namespace webcrypto {

// This class implements ChaCha20-Poly1305, a Authenticated Encryption with
// Associated Data (AEAD) algorithm using the ChaCha20 stream cipher as well as
// the Poly1305 authenticator. This is specified in RFC 8439
// (https://www.rfc-editor.org/rfc/rfc8439).
class ChaCha20Poly1305Implementation : public AlgorithmImplementation {
 public:
  Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override;

  Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override;

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

  const blink::WebCryptoKeyUsageMask all_key_usages_{
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt |
      blink::kWebCryptoKeyUsageWrapKey | blink::kWebCryptoKeyUsageUnwrapKey};
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_CHACHA20_POLY1305_H_
