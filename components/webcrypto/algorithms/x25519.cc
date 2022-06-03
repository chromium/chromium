// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/status.h"

namespace webcrypto {

namespace {

// This class implements the key agreement algorithm using the X25519 function
// based on Curve25519 specified by RFC 7748, to which is also referred as the
// X25519 algorithm by RFC 8410.
//
// TODO(crbug.com/1032821): See also
// https://chromestatus.com/feature/4913922408710144.
class X25519Implementation : public AlgorithmImplementation {
 public:
  Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                    const blink::WebCryptoKey& base_key,
                    bool has_optional_length_bits,
                    unsigned int optional_length_bits,
                    std::vector<uint8_t>* derived_bytes) const override {
    // TODO(crbug.com/1032821): Implement this.
    return Status::ErrorUnsupported();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateX25519Implementation() {
  return std::make_unique<X25519Implementation>();
}

}  // namespace webcrypto
