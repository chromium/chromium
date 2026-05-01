// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/numerics/safe_math.h"
#include "components/webcrypto/algorithms/rsa.h"
#include "components/webcrypto/algorithms/rsa_sign.h"
#include "components/webcrypto/status.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"

namespace webcrypto {

namespace {

class RsaPssImplementation : public RsaHashedAlgorithm {
 public:
  RsaPssImplementation()
      : RsaHashedAlgorithm(blink::kWebCryptoKeyUsageVerify,
                           blink::kWebCryptoKeyUsageSign) {}

  const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const override {
    switch (hash) {
      case blink::kWebCryptoAlgorithmIdSha1:
        return "PS1";
      case blink::kWebCryptoAlgorithmIdSha256:
        return "PS256";
      case blink::kWebCryptoAlgorithmIdSha384:
        return "PS384";
      case blink::kWebCryptoAlgorithmIdSha512:
        return "PS512";
      default:
        return nullptr;
    }
  }

  Status Sign(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              base::span<const uint8_t> data,
              std::vector<uint8_t>* buffer) const override {
    return RsaSign(key, algorithm.RsaPssParams()->SaltLengthBytes(), data,
                   buffer);
  }

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                base::span<const uint8_t> signature,
                base::span<const uint8_t> data,
                bool* signature_match) const override {
    return RsaVerify(key, algorithm.RsaPssParams()->SaltLengthBytes(),
                     signature, data, signature_match);
  }

  bool Supports(blink::WebCryptoOperation op,
                const blink::WebCryptoAlgorithm& algorithm,
                std::optional<unsigned int> length_bits) const override {
    if ((op == blink::kWebCryptoOperationSign) ||
        (op == blink::kWebCryptoOperationVerify)) {
      // There are salt restrictions, but realistically they need to be
      // calculated against the size of the key, which isn't part of the
      // algorithm.
      return true;
    } else {
      return RsaHashedAlgorithm::Supports(op, algorithm, length_bits);
    }
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateRsaPssImplementation() {
  return std::make_unique<RsaPssImplementation>();
}

}  // namespace webcrypto
