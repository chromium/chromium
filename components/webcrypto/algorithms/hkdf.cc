// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithms/secret_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace webcrypto {

namespace {

const blink::WebCryptoKeyUsageMask kValidUsages =
    blink::kWebCryptoKeyUsageDeriveKey | blink::kWebCryptoKeyUsageDeriveBits;

class HkdfImplementation : public AlgorithmImplementation {
 public:
  HkdfImplementation() {}

  Status ImportKey(blink::WebCryptoKeyFormat format,
                   const CryptoData& key_data,
                   const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   blink::WebCryptoKey* key) const override {
    switch (format) {
      case blink::kWebCryptoKeyFormatRaw:
        return ImportKeyRaw(key_data, algorithm, extractable, usages, key);
      default:
        return Status::ErrorUnsupportedImportKeyFormat();
    }
  }

  Status ImportKeyRaw(const CryptoData& key_data,
                      const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      blink::WebCryptoKey* key) const {
    Status status = CheckKeyCreationUsages(kValidUsages, usages);
    if (status.IsError())
      return status;

    if (extractable)
      return Status::ErrorImportExtractableKdfKey();

    return CreateWebCryptoSecretKey(
        key_data,
        blink::WebCryptoKeyAlgorithm::CreateWithoutParams(
            blink::kWebCryptoAlgorithmIdHkdf),
        extractable, usages, key);
  }

  Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                    const blink::WebCryptoKey& base_key,
                    bool has_optional_length_bits,
                    unsigned int optional_length_bits,
                    std::vector<uint8_t>* derived_bytes) const override {
    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    if (!has_optional_length_bits)
      return Status::ErrorHkdfDeriveBitsLengthNotSpecified();

    if (optional_length_bits % 8)
      return Status::ErrorHkdfLengthNotWholeByte();

    const blink::WebCryptoHkdfParams* params = algorithm.HkdfParams();

    const EVP_MD* digest_algorithm = GetDigest(params->GetHash());
    if (!digest_algorithm)
      return Status::ErrorUnsupported();

    // Size output to fit length
    unsigned int derived_bytes_len = optional_length_bits / 8;
    derived_bytes->resize(derived_bytes_len);

    // Algorithm dispatch checks that the algorithm in |base_key| matches
    // |algorithm|.
    const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(base_key);
    if (!HKDF(derived_bytes->data(), derived_bytes_len, digest_algorithm,
              raw_key.data(), raw_key.size(), params->Salt().Data(),
              params->Salt().size(), params->Info().Data(),
              params->Info().size())) {
      uint32_t error = ERR_get_error();
      if (ERR_GET_LIB(error) == ERR_LIB_HKDF &&
          ERR_GET_REASON(error) == HKDF_R_OUTPUT_TOO_LARGE) {
        return Status::ErrorHkdfLengthTooLong();
      }
      return Status::OperationError();
    }

    return Status::Success();
  }

  Status DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                                blink::WebCryptoKeyType type,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                const CryptoData& key_data,
                                blink::WebCryptoKey* key) const override {
    if (algorithm.ParamsType() != blink::kWebCryptoKeyAlgorithmParamsTypeNone ||
        type != blink::kWebCryptoKeyTypeSecret)
      return Status::ErrorUnexpected();

    // NOTE: Unlike ImportKeyRaw(), this does not enforce extractable==false.
    // This is intentional. Although keys cannot currently be created with
    // extractable==true, earlier implementations permitted this, so
    // de-serialization by structured clone should not reject them.
    return CreateWebCryptoSecretKey(key_data, algorithm, extractable, usages,
                                    key);
  }

  Status GetKeyLength(const blink::WebCryptoAlgorithm& key_length_algorithm,
                      bool* has_length_bits,
                      unsigned int* length_bits) const override {
    *has_length_bits = false;
    return Status::Success();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateHkdfImplementation() {
  return std::make_unique<HkdfImplementation>();
}

}  // namespace webcrypto
