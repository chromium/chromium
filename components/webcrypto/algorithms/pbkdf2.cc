// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithms/secret_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace webcrypto {

namespace {

const blink::WebCryptoKeyUsageMask kAllKeyUsages =
    blink::kWebCryptoKeyUsageDeriveKey | blink::kWebCryptoKeyUsageDeriveBits;

class Pbkdf2Implementation : public AlgorithmImplementation {
 public:
  Pbkdf2Implementation() {}

  Status ImportKey(blink::WebCryptoKeyFormat format,
                   base::span<const uint8_t> key_data,
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

  Status ImportKeyRaw(base::span<const uint8_t> key_data,
                      const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      blink::WebCryptoKey* key) const {
    Status status = CheckKeyCreationUsages(kAllKeyUsages, usages);
    if (status.IsError())
      return status;

    if (extractable)
      return Status::ErrorImportExtractableKdfKey();

    const blink::WebCryptoKeyAlgorithm key_algorithm =
        blink::WebCryptoKeyAlgorithm::CreateWithoutParams(
            blink::kWebCryptoAlgorithmIdPbkdf2);

    return CreateWebCryptoSecretKey(key_data, key_algorithm, extractable,
                                    usages, key);
  }

  Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                    const blink::WebCryptoKey& base_key,
                    std::optional<unsigned int> length_bits,
                    std::vector<uint8_t>* derived_bytes) const override {
    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

    if (!length_bits.has_value()) {
      return Status::ErrorPbkdf2DeriveBitsLengthNotSpecified();
    }

    if (*length_bits % 8) {
      return Status::ErrorPbkdf2InvalidLength();
    }

    // According to RFC 2898 "dkLength" (derived key length) is
    // described as being a "positive integer", so it is an error for
    // it to be 0.
    if (*length_bits == 0) {
      return Status::ErrorPbkdf2DeriveBitsLengthZero();
    }

    const blink::WebCryptoPbkdf2Params* params = algorithm.Pbkdf2Params();

    if (params->Iterations() == 0)
      return Status::ErrorPbkdf2Iterations0();

    const EVP_MD* digest_algorithm = GetDigest(params->GetHash());
    if (!digest_algorithm)
      return Status::ErrorUnsupported();

    unsigned int keylen_bytes = *length_bits / 8;
    derived_bytes->resize(keylen_bytes);

    const std::vector<uint8_t>& password = GetSymmetricKeyData(base_key);

    if (!PKCS5_PBKDF2_HMAC(
            reinterpret_cast<const char*>(password.data()), password.size(),
            params->Salt().data(), params->Salt().size(), params->Iterations(),
            digest_algorithm, keylen_bytes, derived_bytes->data())) {
      return Status::OperationError();
    }
    return Status::Success();
  }

  Status DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                                blink::WebCryptoKeyType type,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                base::span<const uint8_t> key_data,
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
                      std::optional<unsigned int>* length_bits) const override {
    *length_bits = std::nullopt;
    return Status::Success();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreatePbkdf2Implementation() {
  return std::make_unique<Pbkdf2Implementation>();
}

}  // namespace webcrypto
