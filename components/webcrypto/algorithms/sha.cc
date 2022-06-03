// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/check_op.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/digest.h"

namespace webcrypto {

namespace {

class ShaImplementation : public AlgorithmImplementation {
 public:
  Status Digest(const blink::WebCryptoAlgorithm& algorithm,
                const CryptoData& data,
                std::vector<uint8_t>* buffer) const override {
    // http://crbug.com/366427: the spec does not define any other failures for
    // digest, so none of the subsequent errors are spec compliant.
    bssl::ScopedEVP_MD_CTX digest_context;
    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

    const EVP_MD* digest_algorithm = GetDigest(algorithm.Id());
    if (!digest_algorithm)
      return Status::ErrorUnsupported();

    if (!EVP_DigestInit_ex(digest_context.get(), digest_algorithm, nullptr))
      return Status::OperationError();

    if (!EVP_DigestUpdate(digest_context.get(), data.bytes(),
                          data.byte_length()))
      return Status::OperationError();

    const size_t hash_expected_size = EVP_MD_CTX_size(digest_context.get());
    if (hash_expected_size == 0)
      return Status::ErrorUnexpected();
    DCHECK_LE(hash_expected_size, static_cast<unsigned>(EVP_MAX_MD_SIZE));

    buffer->resize(hash_expected_size);
    unsigned result_size;  // ignored
    if (!EVP_DigestFinal_ex(digest_context.get(), buffer->data(),
                            &result_size) ||
        result_size != hash_expected_size)
      return Status::OperationError();

    return Status::Success();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateShaImplementation() {
  return std::make_unique<ShaImplementation>();
}

}  // namespace webcrypto
