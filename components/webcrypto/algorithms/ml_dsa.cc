// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/ml_dsa.h"

#include "components/webcrypto/algorithms/asymmetric_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "crypto/evp.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mldsa.h"

namespace webcrypto {

namespace {

const EVP_PKEY_ALG* GetEvpPkeyAlg(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::kWebCryptoAlgorithmIdMlDsa44:
      return EVP_pkey_ml_dsa_44();
    case blink::kWebCryptoAlgorithmIdMlDsa65:
      return EVP_pkey_ml_dsa_65();
    case blink::kWebCryptoAlgorithmIdMlDsa87:
      return EVP_pkey_ml_dsa_87();
    default:
      NOTREACHED();
  }
}

int GetEvpPkeyId(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::kWebCryptoAlgorithmIdMlDsa44:
      return EVP_PKEY_ML_DSA_44;
    case blink::kWebCryptoAlgorithmIdMlDsa65:
      return EVP_PKEY_ML_DSA_65;
    case blink::kWebCryptoAlgorithmIdMlDsa87:
      return EVP_PKEY_ML_DSA_87;
    default:
      NOTREACHED();
  }
}

const char* GetJwkAlg(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::kWebCryptoAlgorithmIdMlDsa44:
      return "ML-DSA-44";
    case blink::kWebCryptoAlgorithmIdMlDsa65:
      return "ML-DSA-65";
    case blink::kWebCryptoAlgorithmIdMlDsa87:
      return "ML-DSA-87";
    default:
      NOTREACHED();
  }
}

// Synthesizes an import algorithm given a key algorithm, so that
// deserialization can reuse the ImportKey*() methods.
blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(algorithm.Id(),
                                                         nullptr);
}

// Set the context for sign/verify, if present. Returns false if context was
// provided and an error was encountered setting the context, true otherwise.
bool SetContext(const blink::WebCryptoAlgorithm& algorithm,
                EVP_PKEY_CTX* pkctx) {
  const blink::WebCryptoContextParams* params = algorithm.ContextParams();
  if (params && params->Context()) {
    if (!EVP_PKEY_CTX_set1_signature_context_string(
            pkctx, params->Context()->data(), params->Context()->size())) {
      return false;
    }
  }
  return true;
}

}  // namespace

Status MlDsaImplementation::GenerateKey(
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

  const EVP_PKEY_ALG* alg = GetEvpPkeyAlg(algorithm.Id());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_generate_from_alg(alg));
  if (!pkey) {
    return Status::OperationError();
  }

  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id());

  blink::WebCryptoKey public_key;
  bssl::UniquePtr<EVP_PKEY> pkey_public(EVP_PKEY_copy_public(pkey.get()));
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

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status MlDsaImplementation::ImportKey(
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
    case blink::kWebCryptoKeyFormatPkcs8:
      return ImportKeyPkcs8(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatSpki:
      return ImportKeySpki(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatJwk:
      return ImportKeyJwk(key_data, algorithm, extractable, usages, key);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status MlDsaImplementation::ExportKey(blink::WebCryptoKeyFormat format,
                                      const blink::WebCryptoKey& key,
                                      std::vector<uint8_t>* buffer) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRawPublic:
      return ExportKeyRawPublic(key, buffer);
    case blink::kWebCryptoKeyFormatRawSeed:
      return ExportKeyRawSeed(key, buffer);
    case blink::kWebCryptoKeyFormatPkcs8:
      return ExportKeyPkcs8(key, buffer);
    case blink::kWebCryptoKeyFormatSpki:
      return ExportKeySpki(key, buffer);
    case blink::kWebCryptoKeyFormatJwk:
      return ExportKeyJwk(key, buffer);
    default:
      return Status::ErrorUnsupportedExportKeyFormat();
  }
}

Status MlDsaImplementation::Sign(const blink::WebCryptoAlgorithm& algorithm,
                                 const blink::WebCryptoKey& key,
                                 base::span<const uint8_t> message,
                                 std::vector<uint8_t>* signature) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate) {
    return Status::ErrorUnexpectedKeyType();
  }

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pkctx;
  if (!EVP_DigestSignInit(ctx.get(), &pkctx, /*type=*/nullptr,
                          /*e=*/nullptr, GetEVP_PKEY(key))) {
    return Status::OperationError();
  }
  if (!SetContext(algorithm, pkctx)) {
    return Status::OperationError();
  }

  // Get the length of the signature.
  size_t sig_len = 0;
  if (!EVP_DigestSign(ctx.get(), nullptr, &sig_len, message.data(),
                      message.size())) {
    return Status::OperationError();
  }

  // Now get the signature.
  signature->resize(sig_len);
  if (!EVP_DigestSign(ctx.get(), signature->data(), &sig_len, message.data(),
                      message.size())) {
    return Status::OperationError();
  }
  signature->resize(sig_len);

  return Status::Success();
}

Status MlDsaImplementation::Verify(const blink::WebCryptoAlgorithm& algorithm,
                                   const blink::WebCryptoKey& key,
                                   base::span<const uint8_t> signature,
                                   base::span<const uint8_t> message,
                                   bool* signature_match) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic) {
    return Status::ErrorUnexpectedKeyType();
  }

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pkctx;
  if (!EVP_DigestVerifyInit(ctx.get(), &pkctx, /*type=*/nullptr,
                            /*e=*/nullptr, GetEVP_PKEY(key))) {
    return Status::OperationError();
  }
  if (!SetContext(algorithm, pkctx)) {
    return Status::OperationError();
  }

  *signature_match =
      1 == EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                            message.data(), message.size());

  return Status::Success();
}

Status MlDsaImplementation::ImportKeyRawPublic(
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

  const EVP_PKEY_ALG* alg = GetEvpPkeyAlg(algorithm.Id());
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

Status MlDsaImplementation::ImportKeyRawSeed(
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

  const EVP_PKEY_ALG* alg = GetEvpPkeyAlg(algorithm.Id());
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

Status MlDsaImplementation::ImportKeyPkcs8(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_private_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  bssl::UniquePtr<EVP_PKEY> private_key;
  status = ImportUnverifiedPkeyFromPkcs8(key_data, GetEvpPkeyId(algorithm.Id()),
                                         &private_key);
  if (status.IsError()) {
    return status;
  }

  return CreateWebCryptoPrivateKey(
      std::move(private_key),
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id()),
      extractable, usages, key);
}

Status MlDsaImplementation::ImportKeySpki(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  bssl::UniquePtr<EVP_PKEY> public_key;
  status = ImportUnverifiedPkeyFromSpki(key_data, GetEvpPkeyId(algorithm.Id()),
                                        &public_key);
  if (status.IsError()) {
    return status;
  }

  return CreateWebCryptoPublicKey(
      std::move(public_key),
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id()),
      extractable, usages, key);
}

Status MlDsaImplementation::ImportKeyJwk(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  JwkReader jwk;
  const char* expected_alg = GetJwkAlg(algorithm.Id());
  Status status = jwk.Init(key_data, extractable, usages, "AKP", expected_alg);
  if (status.IsError()) {
    return status;
  }

  bool is_private_key = jwk.HasMember("priv");
  status = is_private_key
               ? CheckKeyCreationUsages(all_private_key_usages_, usages)
               : CheckKeyCreationUsages(all_public_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  std::vector<uint8_t> raw_public_key;
  status = jwk.GetBytes("pub", &raw_public_key);
  if (status.IsError()) {
    return status;
  }

  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id());
  const EVP_PKEY_ALG* alg = GetEvpPkeyAlg(algorithm.Id());
  bssl::UniquePtr<EVP_PKEY> public_evp_pkey(EVP_PKEY_from_raw_public_key(
      alg, raw_public_key.data(), raw_public_key.size()));
  if (!public_evp_pkey) {
    return Status::DataError();
  }

  if (!is_private_key) {
    // No need to check for a private key, just return the public key.
    return CreateWebCryptoPublicKey(std::move(public_evp_pkey), key_algorithm,
                                    extractable, usages, key);
  }

  std::vector<uint8_t> raw_private_key;
  status = jwk.GetBytes("priv", &raw_private_key);
  if (status.IsError()) {
    return status;
  }
  bssl::UniquePtr<EVP_PKEY> private_evp_pkey(EVP_PKEY_from_private_seed(
      alg, raw_private_key.data(), raw_private_key.size()));
  if (!private_evp_pkey) {
    return Status::DataError();
  }

  // Check the public key matches the private key by comparing the JWK's public
  // key to the JWK's private key, which ensures the public key generated from
  // the private key matches.
  if (!EVP_PKEY_cmp(private_evp_pkey.get(), public_evp_pkey.get())) {
    return Status::DataError();
  }

  return CreateWebCryptoPrivateKey(std::move(private_evp_pkey), key_algorithm,
                                   extractable, usages, key);
}

Status MlDsaImplementation::ExportKeyRawPublic(
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

Status MlDsaImplementation::ExportKeyRawSeed(
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

Status MlDsaImplementation::ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                           std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePrivate) {
    return Status::ErrorUnexpectedKeyType();
  }
  *buffer = crypto::evp::PrivateKeyToBytes(GetEVP_PKEY(key));
  return Status::Success();
}

Status MlDsaImplementation::ExportKeySpki(const blink::WebCryptoKey& key,
                                          std::vector<uint8_t>* buffer) const {
  if (key.GetType() != blink::kWebCryptoKeyTypePublic) {
    return Status::ErrorUnexpectedKeyType();
  }
  *buffer = crypto::evp::PublicKeyToBytes(GetEVP_PKEY(key));
  return Status::Success();
}

Status MlDsaImplementation::ExportKeyJwk(const blink::WebCryptoKey& key,
                                         std::vector<uint8_t>* buffer) const {
  EVP_PKEY* pkey = GetEVP_PKEY(key);
  size_t keylen = 0;
  if (!EVP_PKEY_get_raw_public_key(pkey, nullptr, &keylen)) {
    return Status::OperationError();
  }

  std::vector<uint8_t> raw_public_key(keylen);
  if (!EVP_PKEY_get_raw_public_key(pkey, raw_public_key.data(), &keylen)) {
    return Status::OperationError();
  }
  raw_public_key.resize(keylen);

  const char* jwk_alg = GetJwkAlg(key.Algorithm().Id());
  JwkWriter jwk(jwk_alg, key.Extractable(), key.Usages(), "AKP");

  jwk.SetBytes("pub", raw_public_key);
  if (key.GetType() == blink::kWebCryptoKeyTypePrivate) {
    if (!EVP_PKEY_get_private_seed(pkey, nullptr, &keylen)) {
      return Status::OperationError();
    }

    std::vector<uint8_t> raw_private_key(keylen);
    if (!EVP_PKEY_get_private_seed(pkey, raw_private_key.data(), &keylen)) {
      return Status::OperationError();
    }
    raw_private_key.resize(keylen);

    jwk.SetBytes("priv", raw_private_key);
  }

  jwk.ToJson(buffer);
  return Status::Success();
}

bool MlDsaImplementation::Supports(
    blink::WebCryptoOperation op,
    const blink::WebCryptoAlgorithm& algorithm,
    std::optional<unsigned int> length_bits) const {
  // Boring limits the context to at most 255 bytes
  const blink::WebCryptoContextParams* params = algorithm.ContextParams();
  if (params && params->Context()) {
    return params->Context()->size() < 256;
  }
  // None of the algorithm params have hard constraints.
  return true;
}

Status MlDsaImplementation::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    base::span<const uint8_t> key_data,
    blink::WebCryptoKey* key) const {
  blink::WebCryptoAlgorithm import_algorithm =
      SynthesizeImportAlgorithmForClone(algorithm);

  Status status;

  switch (type) {
    case blink::kWebCryptoKeyTypePublic:
      status =
          ImportKeySpki(key_data, import_algorithm, extractable, usages, key);
      break;
    case blink::kWebCryptoKeyTypePrivate:
      status =
          ImportKeyPkcs8(key_data, import_algorithm, extractable, usages, key);
      break;
    default:
      return Status::ErrorUnexpected();
  }

  if (!status.IsSuccess()) {
    return status;
  }

  if (algorithm.Id() != key->Algorithm().Id()) {
    return Status::ErrorUnexpected();
  }

  if (type != key->GetType()) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

std::unique_ptr<AlgorithmImplementation> CreateMlDsaImplementation() {
  return std::make_unique<MlDsaImplementation>();
}

}  // namespace webcrypto
