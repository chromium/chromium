// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHM_DISPATCH_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHM_DISPATCH_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_crypto.h"

namespace webcrypto {

class GenerateKeyResult;
class EncapsulateKeyResult;
class EncapsulateBitsResult;
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
               base::span<const uint8_t> data,
               std::vector<uint8_t>* buffer);

Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               base::span<const uint8_t> data,
               std::vector<uint8_t>* buffer);

Status Digest(const blink::WebCryptoAlgorithm& algorithm,
              base::span<const uint8_t> data,
              std::vector<uint8_t>* buffer);

Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   GenerateKeyResult* result);

Status ImportKey(blink::WebCryptoKeyFormat format,
                 base::span<const uint8_t> key_data,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key);

Status ExportKey(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 std::vector<uint8_t>* buffer);

Status Sign(const blink::WebCryptoAlgorithm& algorithm,
            const blink::WebCryptoKey& key,
            base::span<const uint8_t> data,
            std::vector<uint8_t>* buffer);

Status Verify(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              base::span<const uint8_t> signature,
              base::span<const uint8_t> data,
              bool* signature_match);

Status WrapKey(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key_to_wrap,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrapping_algorithm,
               std::vector<uint8_t>* buffer);

Status UnwrapKey(blink::WebCryptoKeyFormat format,
                 base::span<const uint8_t> wrapped_key_data,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& wrapping_algorithm,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoKey* key);

Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& base_key,
                  std::optional<unsigned int> length_bits,
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

// Encapsulate and Decapsulate functions are implemented by calling the
// underlying encapsulate/decapsulate/importKey operations, as per the spec
// (https://wicg.github.io/webcrypto-modern-algos/).
Status EncapsulateKey(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& encapsulation_key,
                      const blink::WebCryptoAlgorithm& shared_key_algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      EncapsulateKeyResult* result);

Status EncapsulateBits(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& encapsulation_key,
                       EncapsulateBitsResult* result);

Status DecapsulateKey(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& decapsulation_key,
                      base::span<const uint8_t> ciphertext,
                      const blink::WebCryptoAlgorithm& shared_key_algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      blink::WebCryptoKey* shared_key);

Status DecapsulateBits(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& decapsulation_key,
                       base::span<const uint8_t> ciphertext,
                       std::vector<uint8_t>* shared_bits);

bool SerializeKeyForClone(const blink::WebCryptoKey& key,
                          std::vector<uint8_t>* key_data);

bool DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                            blink::WebCryptoKeyType type,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usages,
                            base::span<const uint8_t> key_data,
                            blink::WebCryptoKey* key);

bool Supports(blink::WebCryptoOperation op,
              const blink::WebCryptoAlgorithm& algorithm,
              std::optional<unsigned int> length_bits);

Status GetKeyLength(const blink::WebCryptoAlgorithm& key_length_algorithm,
                    std::optional<unsigned int>* length_bits);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHM_DISPATCH_H_
