// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/status.h"

namespace webcrypto {

namespace {

// This class implements the Ed25519 algorithm, which is a particular parameter
// choice for the EdDSA algorithm specified by RFC 8032. The underlying curve
// used by Ed25519 is equivalent to Curve25519 specified by RFC 7748.
//
// TODO(crbug.com/1032821): See also
// https://chromestatus.com/feature/4913922408710144.
class Ed25519Implementation : public AlgorithmImplementation {
 public:
  Status Sign(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              const CryptoData& data,
              std::vector<uint8_t>* buffer) const override {
    // TODO(crbug.com/1032821): Implement this.
    return Status::ErrorUnsupported();
  }

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                const CryptoData& signature,
                const CryptoData& data,
                bool* signature_match) const override {
    // TODO(crbug.com/1032821): Implement this.
    return Status::ErrorUnsupported();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateEd25519Implementation() {
  return std::make_unique<Ed25519Implementation>();
}

}  // namespace webcrypto
