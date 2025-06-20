// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/asymmetric_key_util.h"

#include <stdint.h>

#include <utility>

#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"
#include "crypto/evp.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace webcrypto {

Status CreateWebCryptoPublicKey(bssl::UniquePtr<EVP_PKEY> public_key,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key) {
  *key = blink::WebCryptoKey::Create(
      CreateAsymmetricKeyHandle(std::move(public_key)),
      blink::kWebCryptoKeyTypePublic, extractable, algorithm, usages);
  return Status::Success();
}

Status CreateWebCryptoPrivateKey(bssl::UniquePtr<EVP_PKEY> private_key,
                                 const blink::WebCryptoKeyAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 blink::WebCryptoKey* key) {
  *key = blink::WebCryptoKey::Create(
      CreateAsymmetricKeyHandle(std::move(private_key)),
      blink::kWebCryptoKeyTypePrivate, extractable, algorithm, usages);
  return Status::Success();
}

Status ImportUnverifiedPkeyFromSpki(base::span<const uint8_t> key_data,
                                    int expected_pkey_id,
                                    bssl::UniquePtr<EVP_PKEY>* out_pkey) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<EVP_PKEY> pkey = crypto::evp::PublicKeyFromBytes(key_data);
  if (!pkey) {
    return Status::DataError();
  }

  if (EVP_PKEY_id(pkey.get()) != expected_pkey_id)
    return Status::DataError();  // Data did not define expected key type.

  *out_pkey = std::move(pkey);
  return Status::Success();
}

Status ImportUnverifiedPkeyFromPkcs8(base::span<const uint8_t> key_data,
                                     int expected_pkey_id,
                                     bssl::UniquePtr<EVP_PKEY>* out_pkey) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::UniquePtr<EVP_PKEY> pkey = crypto::evp::PrivateKeyFromBytes(key_data);
  if (!pkey) {
    return Status::DataError();
  }

  if (EVP_PKEY_id(pkey.get()) != expected_pkey_id)
    return Status::DataError();  // Data did not define expected key type.

  *out_pkey = std::move(pkey);
  return Status::Success();
}

Status GetUsagesForGenerateAsymmetricKey(
    blink::WebCryptoKeyUsageMask combined_usages,
    blink::WebCryptoKeyUsageMask all_public_usages,
    blink::WebCryptoKeyUsageMask all_private_usages,
    blink::WebCryptoKeyUsageMask* public_usages,
    blink::WebCryptoKeyUsageMask* private_usages) {
  // Ensure that the combined usages is a subset of the total possible usages.
  Status status = CheckKeyCreationUsages(all_public_usages | all_private_usages,
                                         combined_usages);
  if (status.IsError())
    return status;

  *public_usages = combined_usages & all_public_usages;
  *private_usages = combined_usages & all_private_usages;

  // NOTE: empty private_usages is allowed at this layer. Such keys will be
  // rejected at a higher layer.

  return Status::Success();
}

}  // namespace webcrypto
