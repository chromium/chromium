// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/webcrypto/algorithms/asymmetric_key_util.h"

#include <stdint.h>
#include <utility>

#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace webcrypto {

// Exports an EVP_PKEY public key to the SPKI format.
Status ExportPKeySpki(EVP_PKEY* key, std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  uint8_t* der;
  size_t der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) || !EVP_marshal_public_key(cbb.get(), key) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return Status::ErrorUnexpected();
  }
  buffer->assign(der, der + der_len);
  OPENSSL_free(der);
  return Status::Success();
}

// Exports an EVP_PKEY private key to the PKCS8 format.
Status ExportPKeyPkcs8(EVP_PKEY* key, std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // TODO(eroman): Use the OID specified by webcrypto spec.
  //               http://crbug.com/373545
  uint8_t* der;
  size_t der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) || !EVP_marshal_private_key(cbb.get(), key) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return Status::ErrorUnexpected();
  }
  buffer->assign(der, der + der_len);
  OPENSSL_free(der);
  return Status::Success();
}

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

  CBS cbs;
  CBS_init(&cbs, key_data.data(), key_data.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0)
    return Status::DataError();

  if (EVP_PKEY_id(pkey.get()) != expected_pkey_id)
    return Status::DataError();  // Data did not define expected key type.

  *out_pkey = std::move(pkey);
  return Status::Success();
}

Status ImportUnverifiedPkeyFromPkcs8(base::span<const uint8_t> key_data,
                                     int expected_pkey_id,
                                     bssl::UniquePtr<EVP_PKEY>* out_pkey) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs;
  CBS_init(&cbs, key_data.data(), key_data.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_private_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0)
    return Status::DataError();

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
