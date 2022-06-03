// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/check_op.h"
#include "base/numerics/safe_math.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithms/secret_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/secure_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"

namespace webcrypto {

namespace {

Status GetDigestBlockSizeBits(const blink::WebCryptoAlgorithm& algorithm,
                              unsigned int* block_size_bits) {
  const EVP_MD* md = GetDigest(algorithm);
  if (!md)
    return Status::ErrorUnsupported();
  *block_size_bits = static_cast<unsigned int>(8 * EVP_MD_block_size(md));
  return Status::Success();
}

// Gets the requested key length in bits for an HMAC import operation.
Status GetHmacImportKeyLengthBits(
    const blink::WebCryptoHmacImportParams* params,
    unsigned int key_data_byte_length,
    unsigned int* keylen_bits) {
  if (key_data_byte_length == 0)
    return Status::ErrorHmacImportEmptyKey();

  // Make sure that the key data's length can be represented in bits without
  // overflow.
  base::CheckedNumeric<unsigned int> checked_keylen_bits(key_data_byte_length);
  checked_keylen_bits *= 8;

  if (!checked_keylen_bits.IsValid())
    return Status::ErrorDataTooLarge();

  unsigned int data_keylen_bits = checked_keylen_bits.ValueOrDie();

  // Determine how many bits of the input to use.
  *keylen_bits = data_keylen_bits;
  if (params->HasLengthBits()) {
    // The requested bit length must be:
    //   * No longer than the input data length
    //   * At most 7 bits shorter.
    if (NumBitsToBytes(params->OptionalLengthBits()) != key_data_byte_length)
      return Status::ErrorHmacImportBadLength();
    *keylen_bits = params->OptionalLengthBits();
  }

  return Status::Success();
}

const char* GetJwkHmacAlgorithmName(blink::WebCryptoAlgorithmId hash) {
  switch (hash) {
    case blink::kWebCryptoAlgorithmIdSha1:
      return "HS1";
    case blink::kWebCryptoAlgorithmIdSha256:
      return "HS256";
    case blink::kWebCryptoAlgorithmIdSha384:
      return "HS384";
    case blink::kWebCryptoAlgorithmIdSha512:
      return "HS512";
    default:
      return nullptr;
  }
}

const blink::WebCryptoKeyUsageMask kAllKeyUsages =
    blink::kWebCryptoKeyUsageSign | blink::kWebCryptoKeyUsageVerify;

Status SignHmac(const std::vector<uint8_t>& raw_key,
                const blink::WebCryptoAlgorithm& hash,
                const CryptoData& data,
                std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_MD* digest_algorithm = GetDigest(hash);
  if (!digest_algorithm)
    return Status::ErrorUnsupported();
  size_t hmac_expected_length = EVP_MD_size(digest_algorithm);

  buffer->resize(hmac_expected_length);

  unsigned int hmac_actual_length;
  if (!HMAC(digest_algorithm, raw_key.data(), raw_key.size(), data.bytes(),
            data.byte_length(), buffer->data(), &hmac_actual_length)) {
    return Status::OperationError();
  }

  // HMAC() promises to use at most EVP_MD_CTX_size(). If this was not the
  // case then memory corruption may have just occurred.
  CHECK_EQ(hmac_expected_length, hmac_actual_length);

  return Status::Success();
}

class HmacImplementation : public AlgorithmImplementation {
 public:
  HmacImplementation() {}

  Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usages,
                     GenerateKeyResult* result) const override {
    Status status = CheckKeyCreationUsages(kAllKeyUsages, usages);
    if (status.IsError())
      return status;

    const blink::WebCryptoHmacKeyGenParams* params =
        algorithm.HmacKeyGenParams();

    unsigned int keylen_bits = 0;
    if (params->HasLengthBits()) {
      keylen_bits = params->OptionalLengthBits();
      // Zero-length HMAC keys are disallowed by the spec.
      if (keylen_bits == 0)
        return Status::ErrorGenerateHmacKeyLengthZero();
    } else {
      status = GetDigestBlockSizeBits(params->GetHash(), &keylen_bits);
      if (status.IsError())
        return status;
    }

    return GenerateWebCryptoSecretKey(blink::WebCryptoKeyAlgorithm::CreateHmac(
                                          params->GetHash().Id(), keylen_bits),
                                      extractable, usages, keylen_bits, result);
  }

  Status ImportKey(blink::WebCryptoKeyFormat format,
                   const CryptoData& key_data,
                   const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   blink::WebCryptoKey* key) const override {
    switch (format) {
      case blink::kWebCryptoKeyFormatRaw:
        return ImportKeyRaw(key_data, algorithm, extractable, usages, key);
      case blink::kWebCryptoKeyFormatJwk:
        return ImportKeyJwk(key_data, algorithm, extractable, usages, key);
      default:
        return Status::ErrorUnsupportedImportKeyFormat();
    }
  }

  Status ExportKey(blink::WebCryptoKeyFormat format,
                   const blink::WebCryptoKey& key,
                   std::vector<uint8_t>* buffer) const override {
    switch (format) {
      case blink::kWebCryptoKeyFormatRaw:
        return ExportKeyRaw(key, buffer);
      case blink::kWebCryptoKeyFormatJwk:
        return ExportKeyJwk(key, buffer);
      default:
        return Status::ErrorUnsupportedExportKeyFormat();
    }
  }

  Status ImportKeyRaw(const CryptoData& key_data,
                      const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      blink::WebCryptoKey* key) const {
    Status status = CheckKeyCreationUsages(kAllKeyUsages, usages);
    if (status.IsError())
      return status;

    const blink::WebCryptoHmacImportParams* params =
        algorithm.HmacImportParams();

    unsigned int keylen_bits = 0;
    status = GetHmacImportKeyLengthBits(params, key_data.byte_length(),
                                        &keylen_bits);
    if (status.IsError())
      return status;

    const blink::WebCryptoKeyAlgorithm key_algorithm =
        blink::WebCryptoKeyAlgorithm::CreateHmac(params->GetHash().Id(),
                                                 keylen_bits);

    // If no bit truncation was requested, then done!
    if ((keylen_bits % 8) == 0) {
      return CreateWebCryptoSecretKey(key_data, key_algorithm, extractable,
                                      usages, key);
    }

    // Otherwise zero out the unused bits in the key data before importing.
    std::vector<uint8_t> modified_key_data(
        key_data.bytes(), key_data.bytes() + key_data.byte_length());
    TruncateToBitLength(keylen_bits, &modified_key_data);
    return CreateWebCryptoSecretKey(CryptoData(modified_key_data),
                                    key_algorithm, extractable, usages, key);
  }

  Status ImportKeyJwk(const CryptoData& key_data,
                      const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      blink::WebCryptoKey* key) const {
    Status status = CheckKeyCreationUsages(kAllKeyUsages, usages);
    if (status.IsError())
      return status;

    const char* algorithm_name =
        GetJwkHmacAlgorithmName(algorithm.HmacImportParams()->GetHash().Id());
    if (!algorithm_name)
      return Status::ErrorUnexpected();

    std::vector<uint8_t> raw_data;
    JwkReader jwk;
    status = ReadSecretKeyNoExpectedAlgJwk(key_data, extractable, usages,
                                           &raw_data, &jwk);
    if (status.IsError())
      return status;
    status = jwk.VerifyAlg(algorithm_name);
    if (status.IsError())
      return status;

    return ImportKeyRaw(CryptoData(raw_data), algorithm, extractable, usages,
                        key);
  }

  Status ExportKeyRaw(const blink::WebCryptoKey& key,
                      std::vector<uint8_t>* buffer) const {
    *buffer = GetSymmetricKeyData(key);
    return Status::Success();
  }

  Status ExportKeyJwk(const blink::WebCryptoKey& key,
                      std::vector<uint8_t>* buffer) const {
    const std::vector<uint8_t>& raw_data = GetSymmetricKeyData(key);

    const char* algorithm_name =
        GetJwkHmacAlgorithmName(key.Algorithm().HmacParams()->GetHash().Id());
    if (!algorithm_name)
      return Status::ErrorUnexpected();

    WriteSecretKeyJwk(CryptoData(raw_data), algorithm_name, key.Extractable(),
                      key.Usages(), buffer);

    return Status::Success();
  }

  Status Sign(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              const CryptoData& data,
              std::vector<uint8_t>* buffer) const override {
    const blink::WebCryptoAlgorithm& hash =
        key.Algorithm().HmacParams()->GetHash();

    return SignHmac(GetSymmetricKeyData(key), hash, data, buffer);
  }

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                const CryptoData& signature,
                const CryptoData& data,
                bool* signature_match) const override {
    std::vector<uint8_t> result;
    Status status = Sign(algorithm, key, data, &result);

    if (status.IsError())
      return status;

    // Do not allow verification of truncated MACs.
    *signature_match = result.size() == signature.byte_length() &&
                       crypto::SecureMemEqual(result.data(), signature.bytes(),
                                              signature.byte_length());

    return Status::Success();
  }

  Status DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                                blink::WebCryptoKeyType type,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                const CryptoData& key_data,
                                blink::WebCryptoKey* key) const override {
    if (algorithm.ParamsType() != blink::kWebCryptoKeyAlgorithmParamsTypeHmac ||
        type != blink::kWebCryptoKeyTypeSecret)
      return Status::ErrorUnexpected();

    return CreateWebCryptoSecretKey(key_data, algorithm, extractable, usages,
                                    key);
  }

  Status GetKeyLength(const blink::WebCryptoAlgorithm& key_length_algorithm,
                      bool* has_length_bits,
                      unsigned int* length_bits) const override {
    const blink::WebCryptoHmacImportParams* params =
        key_length_algorithm.HmacImportParams();

    *has_length_bits = true;
    if (params->HasLengthBits()) {
      *length_bits = params->OptionalLengthBits();
      if (*length_bits == 0)
        return Status::ErrorGetHmacKeyLengthZero();
      return Status::Success();
    }

    return GetDigestBlockSizeBits(params->GetHash(), length_bits);
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateHmacImplementation() {
  return std::make_unique<HmacImplementation>();
}

}  // namespace webcrypto
