// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/mlkem768_x25519.h"

#include "components/webcrypto/algorithms/asymmetric_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace webcrypto {

namespace {

blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(algorithm.Id(),
                                                         nullptr);
}

}  // namespace

Status MlKem768X25519Implementation::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask combined_usages,
    GenerateKeyResult* result) const {
  blink::WebCryptoKeyUsageMask public_usages = 0;
  blink::WebCryptoKeyUsageMask private_usages = 0;

  Status status = GetUsagesForGenerateAsymmetricKey(
      combined_usages, all_public_key_usages_, all_private_key_usages_,
      &public_usages, &private_usages);
  if (status.IsError()) {
    return status;
  }

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_PKEY_ALG* alg = EVP_pkey_xwing();
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_generate_from_alg(alg));
  if (!pkey) {
    return Status::OperationError();
  }

  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id());

  blink::WebCryptoKey public_key;
  bssl::UniquePtr<EVP_PKEY> pkey_public(EVP_PKEY_copy_public(pkey.get()));
  if (!pkey_public) {
    return Status::OperationError();
  }
  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always extractable.
  status = CreateWebCryptoPublicKey(std::move(pkey_public), key_algorithm,
                                    /*extractable=*/true, public_usages,
                                    &public_key);
  if (status.IsError()) {
    return status;
  }

  blink::WebCryptoKey private_key;
  status = CreateWebCryptoPrivateKey(std::move(pkey), key_algorithm,
                                     extractable, private_usages, &private_key);
  if (status.IsError()) {
    return status;
  }

  result->AssignKeyPair(/*public_key=*/public_key, /*private_key=*/private_key);
  return Status::Success();
}

Status MlKem768X25519Implementation::ImportKey(
    blink::WebCryptoKeyFormat format,
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRawPublic:
      return ImportKeyRawPublic(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatRawSeed:
      return ImportKeyRawSeed(key_data, algorithm, extractable, usages, key);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status MlKem768X25519Implementation::ExportKey(
    blink::WebCryptoKeyFormat format,
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRawPublic:
      return ExportKeyRawPublic(key, buffer);
    case blink::kWebCryptoKeyFormatRawSeed:
      return ExportKeyRawSeed(key, buffer);
    default:
      return Status::ErrorUnsupportedExportKeyFormat();
  }
}

Status MlKem768X25519Implementation::GetPublicKey(
    const blink::WebCryptoKey& key,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* public_key) const {
  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  bssl::UniquePtr<EVP_PKEY> pub_pkey(EVP_PKEY_copy_public(GetEVP_PKEY(key)));
  if (!pub_pkey) {
    return Status::OperationError();
  }

  return CreateWebCryptoPublicKey(std::move(pub_pkey), key.Algorithm(), true,
                                  usages, public_key);
}

Status MlKem768X25519Implementation::Encapsulate(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& encapsulation_key,
    std::vector<uint8_t>* out_shared_secret,
    std::vector<uint8_t>* out_ciphertext) const {
  if (encapsulation_key.GetType() != blink::kWebCryptoKeyTypePublic) {
    return Status::ErrorUnexpectedKeyType();
  }

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_KEM* kem = EVP_kem_xwing();
  out_ciphertext->resize(EVP_KEM_ciphertext_len(kem));
  out_shared_secret->resize(EVP_KEM_secret_len(kem));

  if (!EVP_KEM_encap(kem, out_ciphertext->data(), out_ciphertext->size(),
                     out_shared_secret->data(), out_shared_secret->size(),
                     GetEVP_PKEY(encapsulation_key))) {
    return Status::OperationError();
  }

  return Status::Success();
}

Status MlKem768X25519Implementation::Decapsulate(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& decapsulation_key,
    base::span<const uint8_t> ciphertext,
    std::vector<uint8_t>* out_shared_secret) const {
  if (decapsulation_key.GetType() != blink::kWebCryptoKeyTypePrivate) {
    return Status::ErrorUnexpectedKeyType();
  }

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_KEM* kem = EVP_kem_xwing();
  if (ciphertext.size() != EVP_KEM_ciphertext_len(kem)) {
    return Status::DataError();
  }

  out_shared_secret->resize(EVP_KEM_secret_len(kem));

  if (!EVP_KEM_decap(kem, out_shared_secret->data(), out_shared_secret->size(),
                     ciphertext.data(), ciphertext.size(),
                     GetEVP_PKEY(decapsulation_key))) {
    return Status::OperationError();
  }

  return Status::Success();
}

Status MlKem768X25519Implementation::SerializeKeyForClone(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* key_data) const {
  switch (key.GetType()) {
    case blink::kWebCryptoKeyTypePublic:
      return ExportKeyRawPublic(key, key_data);
    case blink::kWebCryptoKeyTypePrivate:
      return ExportKeyRawSeed(key, key_data);
    default:
      return Status::ErrorUnexpected();
  }
}

Status MlKem768X25519Implementation::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    base::span<const uint8_t> key_data,
    blink::WebCryptoKey* key) const {
  blink::WebCryptoAlgorithm import_algorithm =
      SynthesizeImportAlgorithmForClone(algorithm);

  switch (type) {
    case blink::kWebCryptoKeyTypePublic:
      return ImportKeyRawPublic(key_data, import_algorithm, extractable, usages,
                                key);
    case blink::kWebCryptoKeyTypePrivate:
      return ImportKeyRawSeed(key_data, import_algorithm, extractable, usages,
                              key);
    default:
      return Status::ErrorUnexpected();
  }
}

Status MlKem768X25519Implementation::ImportKeyRawPublic(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  const EVP_PKEY_ALG* alg = EVP_pkey_xwing();
  bssl::UniquePtr<EVP_PKEY> pkey(
      EVP_PKEY_from_raw_public_key(alg, key_data.data(), key_data.size()));
  if (!pkey) {
    return Status::DataError();
  }

  return CreateWebCryptoPublicKey(
      std::move(pkey),
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id()),
      extractable, usages, key);
}

Status MlKem768X25519Implementation::ImportKeyRawSeed(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  Status status = CheckKeyCreationUsages(all_private_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  const EVP_PKEY_ALG* alg = EVP_pkey_xwing();
  bssl::UniquePtr<EVP_PKEY> pkey(
      EVP_PKEY_from_private_seed(alg, key_data.data(), key_data.size()));
  if (!pkey) {
    return Status::DataError();
  }

  return CreateWebCryptoPrivateKey(
      std::move(pkey),
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id()),
      extractable, usages, key);
}

Status MlKem768X25519Implementation::ExportKeyRawPublic(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic) {
    return Status::ErrorUnexpectedKeyType();
  }

  size_t len = 0;
  if (!EVP_PKEY_get_raw_public_key(GetEVP_PKEY(key), nullptr, &len)) {
    return Status::OperationError();
  }
  buffer->resize(len);
  if (!EVP_PKEY_get_raw_public_key(GetEVP_PKEY(key), buffer->data(), &len)) {
    return Status::OperationError();
  }
  buffer->resize(len);

  return Status::Success();
}

Status MlKem768X25519Implementation::ExportKeyRawSeed(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate) {
    return Status::ErrorUnexpectedKeyType();
  }

  size_t len = 0;
  if (!EVP_PKEY_get_private_seed(GetEVP_PKEY(key), nullptr, &len)) {
    return Status::OperationError();
  }
  buffer->resize(len);
  if (!EVP_PKEY_get_private_seed(GetEVP_PKEY(key), buffer->data(), &len)) {
    return Status::OperationError();
  }
  buffer->resize(len);

  return Status::Success();
}

std::unique_ptr<AlgorithmImplementation> CreateMlKem768X25519Implementation() {
  return std::make_unique<MlKem768X25519Implementation>();
}

}  // namespace webcrypto
