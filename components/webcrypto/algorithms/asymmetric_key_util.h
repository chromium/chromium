// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_ASYMMETRIC_KEY_UTIL_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_ASYMMETRIC_KEY_UTIL_H_

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/boringssl/src/include/openssl/base.h"

// This file contains functions shared by multiple asymmetric key algorithms.

namespace webcrypto {

class Status;

// Exports an EVP_PKEY public key to the SPKI format.
Status ExportPKeySpki(EVP_PKEY* key, std::vector<uint8_t>* buffer);

// Exports an EVP_PKEY private key to the PKCS8 format.
Status ExportPKeyPkcs8(EVP_PKEY* key, std::vector<uint8_t>* buffer);

// Creates a WebCrypto public key given an EVP_PKEY. This step includes
// exporting the key to SPKI format, for use by serialization later.
Status CreateWebCryptoPublicKey(bssl::UniquePtr<EVP_PKEY> public_key,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key);

// Creates a WebCrypto private key given an EVP_PKEY. This step includes
// exporting the key to PKCS8 format, for use by serialization later.
Status CreateWebCryptoPrivateKey(bssl::UniquePtr<EVP_PKEY> private_key,
                                 const blink::WebCryptoKeyAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 blink::WebCryptoKey* key);

// Imports SPKI bytes to an EVP_PKEY for a public key. The resulting asymmetric
// key may be invalid, and should be verified using something like
// RSA_check_key(). The only validation performed by this function is to ensure
// the key type matched |expected_pkey_id|.
Status ImportUnverifiedPkeyFromSpki(base::span<const uint8_t> key_data,
                                    int expected_pkey_id,
                                    bssl::UniquePtr<EVP_PKEY>* pkey);

// Imports PKCS8 bytes to an EVP_PKEY for a private key. The resulting
// asymmetric key may be invalid, and should be verified using something like
// RSA_check_key(). The only validation performed by this function is to ensure
// the key type matched |expected_pkey_id|.
Status ImportUnverifiedPkeyFromPkcs8(base::span<const uint8_t> key_data,
                                     int expected_pkey_id,
                                     bssl::UniquePtr<EVP_PKEY>* pkey);

// Splits the combined usages given to GenerateKey() into the respective usages
// for the public key and private key. Returns an error if the usages are
// invalid.
Status GetUsagesForGenerateAsymmetricKey(
    blink::WebCryptoKeyUsageMask combined_usages,
    blink::WebCryptoKeyUsageMask all_public_usages,
    blink::WebCryptoKeyUsageMask all_private_usages,
    blink::WebCryptoKeyUsageMask* public_usages,
    blink::WebCryptoKeyUsageMask* private_usages);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_ASYMMETRIC_KEY_UTIL_H_
