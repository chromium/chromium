// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/chacha20_poly1305.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "components/webcrypto/algorithms/secret_key_util.h"
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

namespace webcrypto {

namespace {
// Synthesizes an import algorithm given a key algorithm, so that
// deserialization can reuse the ImportKey*() methods.
blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(algorithm.Id(),
                                                         nullptr);
}
}  // namespace

Status ChaCha20Poly1305Implementation::Encrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    base::span<const uint8_t> data,
    std::vector<uint8_t>* buffer) const {
  const blink::WebCryptoAeadParams* params = algorithm.AeadParams();
  const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(key);

  if (params->Iv().size() != 12) {
    return Status::ErrorIncorrectSizeChaCha20Poly1305Iv();
  }
  unsigned int tag_length_bits = 128;
  if (params->HasTagLengthBits()) {
    tag_length_bits = params->OptionalTagLengthBits();
    if (tag_length_bits != 128) {
      return Status::ErrorInvalidChaCha20Poly1305TagLength();
    }
  }

  return AeadEncryptDecrypt(ENCRYPT, raw_key, data, tag_length_bits / 8,
                            params->Iv(), params->OptionalAdditionalData(),
                            EVP_aead_chacha20_poly1305(), buffer);
}

Status ChaCha20Poly1305Implementation::Decrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    base::span<const uint8_t> data,
    std::vector<uint8_t>* buffer) const {
  const blink::WebCryptoAeadParams* params = algorithm.AeadParams();
  const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(key);

  if (params->Iv().size() != 12) {
    return Status::ErrorIncorrectSizeChaCha20Poly1305Iv();
  }
  unsigned int tag_length_bits = 128;
  if (params->HasTagLengthBits()) {
    tag_length_bits = params->OptionalTagLengthBits();
    if (tag_length_bits != 128) {
      return Status::ErrorInvalidChaCha20Poly1305TagLength();
    }
  }

  return AeadEncryptDecrypt(DECRYPT, raw_key, data, tag_length_bits / 8,
                            params->Iv(), params->OptionalAdditionalData(),
                            EVP_aead_chacha20_poly1305(), buffer);
}

Status ChaCha20Poly1305Implementation::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    GenerateKeyResult* result) const {
  Status status = CheckKeyCreationUsages(all_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  return GenerateWebCryptoSecretKey(
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id()),
      extractable, usages, /*keylen_bits=*/256, result);
}

Status ChaCha20Poly1305Implementation::ImportKey(
    blink::WebCryptoKeyFormat format,
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRawSecret:
      return ImportKeyRaw(key_data, algorithm, extractable, usages, key);
    case blink::kWebCryptoKeyFormatJwk:
      return ImportKeyJwk(key_data, algorithm, extractable, usages, key);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status ChaCha20Poly1305Implementation::ImportKeyRaw(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  const size_t keylen_bytes = key_data.size();
  // Must be 256 bits
  if (keylen_bytes != 32) {
    return Status::ErrorImportChaCha20Poly1305KeyLength();
  }

  return CreateWebCryptoSecretKey(
      key_data,
      blink::WebCryptoKeyAlgorithm::CreateWithoutParams(algorithm.Id()),
      extractable, usages, key);
}

Status ChaCha20Poly1305Implementation::ImportKeyJwk(
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  Status status = CheckKeyCreationUsages(all_key_usages_, usages);
  if (status.IsError()) {
    return status;
  }

  std::vector<uint8_t> raw_data;
  JwkReader jwk;
  status = ReadSecretKeyNoExpectedAlgJwk(key_data, extractable, usages,
                                         &raw_data, &jwk);
  if (status.IsError()) {
    return status;
  }

  bool has_jwk_alg;
  std::string jwk_alg;
  status = jwk.GetAlg(&jwk_alg, &has_jwk_alg);
  if (status.IsError()) {
    return status;
  }

  if (has_jwk_alg) {
    if (jwk_alg != "C20P") {
      return Status::ErrorJwkAlgorithmInconsistent();
    }
  }

  return ImportKeyRaw(raw_data, algorithm, extractable, usages, key);
}

Status ChaCha20Poly1305Implementation::ExportKey(
    blink::WebCryptoKeyFormat format,
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  switch (format) {
    case blink::kWebCryptoKeyFormatRawSecret:
      return ExportKeyRaw(key, buffer);
    case blink::kWebCryptoKeyFormatJwk:
      return ExportKeyJwk(key, buffer);
    default:
      return Status::ErrorUnsupportedExportKeyFormat();
  }
}

Status ChaCha20Poly1305Implementation::ExportKeyRaw(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  *buffer = GetSymmetricKeyData(key);
  return Status::Success();
}

Status ChaCha20Poly1305Implementation::ExportKeyJwk(
    const blink::WebCryptoKey& key,
    std::vector<uint8_t>* buffer) const {
  const std::vector<uint8_t>& raw_data = GetSymmetricKeyData(key);
  WriteSecretKeyJwk(raw_data, "C20P", key.Extractable(), key.Usages(), buffer);

  return Status::Success();
}

Status ChaCha20Poly1305Implementation::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    base::span<const uint8_t> key_data,
    blink::WebCryptoKey* key) const {
  if (algorithm.ParamsType() != blink::kWebCryptoKeyAlgorithmParamsTypeNone ||
      type != blink::kWebCryptoKeyTypeSecret) {
    return Status::ErrorUnexpected();
  }

  return ImportKeyRaw(key_data, SynthesizeImportAlgorithmForClone(algorithm),
                      extractable, usages, key);
}

Status ChaCha20Poly1305Implementation::GetKeyLength(
    const blink::WebCryptoAlgorithm& key_length_algorithm,
    std::optional<unsigned int>* length_bits) const {
  *length_bits = 256;
  return Status::Success();
}

std::unique_ptr<AlgorithmImplementation>
CreateChaCha20Poly1305Implementation() {
  return std::make_unique<ChaCha20Poly1305Implementation>();
}

}  // namespace webcrypto
