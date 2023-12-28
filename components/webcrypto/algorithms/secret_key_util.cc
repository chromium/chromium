// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/secret_key_util.h"

#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace webcrypto {

Status GenerateWebCryptoSecretKey(const blink::WebCryptoKeyAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usages,
                                  unsigned int keylen_bits,
                                  GenerateKeyResult* result) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  unsigned int keylen_bytes = NumBitsToBytes(keylen_bits);
  std::vector<uint8_t> random_bytes(keylen_bytes, 0);

  if (keylen_bytes > 0) {
    if (!RAND_bytes(random_bytes.data(), keylen_bytes))
      return Status::OperationError();
    TruncateToBitLength(keylen_bits, &random_bytes);
  }

  result->AssignSecretKey(blink::WebCryptoKey::Create(
      CreateSymmetricKeyHandle(random_bytes), blink::kWebCryptoKeyTypeSecret,
      extractable, algorithm, usages));

  return Status::Success();
}

Status CreateWebCryptoSecretKey(base::span<const uint8_t> key_data,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key) {
  *key = blink::WebCryptoKey::Create(CreateSymmetricKeyHandle(key_data),
                                     blink::kWebCryptoKeyTypeSecret,
                                     extractable, algorithm, usages);
  return Status::Success();
}

void WriteSecretKeyJwk(base::span<const uint8_t> raw_key_data,
                       std::string_view algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usages,
                       std::vector<uint8_t>* jwk_key_data) {
  JwkWriter writer(algorithm, extractable, usages, "oct");
  writer.SetBytes("k", raw_key_data);
  writer.ToJson(jwk_key_data);
}

Status ReadSecretKeyNoExpectedAlgJwk(
    base::span<const uint8_t> key_data,
    bool expected_extractable,
    blink::WebCryptoKeyUsageMask expected_usages,
    std::vector<uint8_t>* raw_key_data,
    JwkReader* jwk) {
  Status status = jwk->Init(key_data, expected_extractable, expected_usages,
                            "oct", std::string());
  if (status.IsError())
    return status;

  return jwk->GetBytes("k", raw_key_data);
}

}  // namespace webcrypto
