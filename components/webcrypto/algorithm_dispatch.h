// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHM_DISPATCH_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHM_DISPATCH_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "third_party/blink/public/platform/web_crypto.h"

namespace webcrypto {

class CryptoData;
class GenerateKeyResult;
class Status;

// These functions provide an entry point for synchronous webcrypto operations.
//
// The inputs to these methods come from Blink, and hence the validations done
// by Blink can be assumed:
//
//   * The algorithm parameters are consistent with the algorithm
//   * The key contains the required usage for the operation

Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               std::vector<uint8_t>* buffer);

Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               std::vector<uint8_t>* buffer);

Status Digest(const blink::WebCryptoAlgorithm& algorithm,
              const CryptoData& data,
              std::vector<uint8_t>* buffer);

Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   GenerateKeyResult* result);

Status ImportKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& key_data,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key);

Status ExportKey(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 std::vector<uint8_t>* buffer);

Status Sign(const blink::WebCryptoAlgorithm& algorithm,
            const blink::WebCryptoKey& key,
            const CryptoData& data,
            std::vector<uint8_t>* buffer);

Status Verify(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              const CryptoData& signature,
              const CryptoData& data,
              bool* signature_match);

Status WrapKey(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key_to_wrap,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrapping_algorithm,
               std::vector<uint8_t>* buffer);

Status UnwrapKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& wrapped_key_data,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& wrapping_algorithm,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key);

Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& base_key,
                  unsigned int length_bits,
                  std::vector<uint8_t>* derived_bytes);

// Derives a key by calling the underlying deriveBits/getKeyLength/importKey
// operations.
//
// Note that whereas the WebCrypto spec uses a single "derivedKeyType"
// AlgorithmIdentifier in its specification of deriveKey(), here two separate
// AlgorithmIdentifiers are used:
//
//   * |import_algorithm|  -- The parameters required by the derived key's
//                            "importKey" operation.
//
//   * |key_length_algorithm| -- The parameters required by the derived key's
//                               "get key length" operation.
//
// WebCryptoAlgorithm is not a flexible type like AlgorithmIdentifier (it cannot
// be easily re-interpreted as a different parameter type).
//
// Therefore being provided with separate parameter types for the import
// parameters and the key length parameters simplifies passing the right
// parameters onto ImportKey() and GetKeyLength() respectively.
Status DeriveKey(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& base_key,
                 const blink::WebCryptoAlgorithm& import_algorithm,
                 const blink::WebCryptoAlgorithm& key_length_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* derived_key);

bool SerializeKeyForClone(const blink::WebCryptoKey& key,
                          blink::WebVector<uint8_t>* key_data);

bool DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                            blink::WebCryptoKeyType type,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usages,
                            const CryptoData& key_data,
                            blink::WebCryptoKey* key);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHM_DISPATCH_H_
