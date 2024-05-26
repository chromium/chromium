// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_SECRET_KEY_UTIL_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_SECRET_KEY_UTIL_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_key.h"

// This file contains functions shared by multiple symmetric key algorithms.

namespace webcrypto {

class GenerateKeyResult;
class JwkReader;
class Status;

// Generates a random secret key of the given bit length. If the bit length is
// not a multiple of 8, then the resulting key will have ceil(keylen_bits / 8)
// bytes, and the "unused" bits will be set to zero. This function does not do
// any validation checks on the provided parameters.
Status GenerateWebCryptoSecretKey(const blink::WebCryptoKeyAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usages,
                                  unsigned int keylen_bits,
                                  GenerateKeyResult* result);

// Creates a WebCrypto secret key given the raw data. The provided |key_data|
// will be copied into the new key. This function does not do any validation
// checks for the provided parameters.
Status CreateWebCryptoSecretKey(base::span<const uint8_t> key_data,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key);

// Writes a JWK-formatted symmetric key to |jwk_key_data|.
//  * raw_key_data: The actual key data
//  * algorithm: The JWK algorithm name (i.e. "alg")
//  * extractable: The JWK extractability (i.e. "ext")
//  * usages: The JWK usages (i.e. "key_ops")
void WriteSecretKeyJwk(base::span<const uint8_t> raw_key_data,
                       std::string_view algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usages,
                       std::vector<uint8_t>* jwk_key_data);

// Parses a UTF-8 encoded JWK (key_data), and extracts the key material to
// |*raw_key_data|. Returns Status::Success() on success, otherwise an error.
// In order for this to succeed:
//   * expected_extractable must be consistent with the JWK's "ext", if
//     present.
//   * expected_usages must be a subset of the JWK's "key_ops" if present.
Status ReadSecretKeyNoExpectedAlgJwk(
    base::span<const uint8_t> key_data,
    bool expected_extractable,
    blink::WebCryptoKeyUsageMask expected_usages,
    std::vector<uint8_t>* raw_key_data,
    JwkReader* jwk);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_SECRET_KEY_UTIL_H_
