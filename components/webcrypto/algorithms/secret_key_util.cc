// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/secret_key_util.h"

#include "base/record_replay.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

#include <sys/random.h>

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
    // Avoid calling RAND_bytes when recording/replaying as it can behave in
    // non-deterministic ways.
    if (recordreplay::IsRecordingOrReplaying()) {
      if (getrandom(random_bytes.data(), keylen_bytes, 0) != keylen_bytes)
        return Status::OperationError();
    } else {
      if (!RAND_bytes(random_bytes.data(), keylen_bytes))
        return Status::OperationError();
    }
    TruncateToBitLength(keylen_bits, &random_bytes);
  }

  result->AssignSecretKey(blink::WebCryptoKey::Create(
      CreateSymmetricKeyHandle(CryptoData(random_bytes)),
      blink::kWebCryptoKeyTypeSecret, extractable, algorithm, usages));

  return Status::Success();
}

Status CreateWebCryptoSecretKey(const CryptoData& key_data,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key) {
  *key = blink::WebCryptoKey::Create(CreateSymmetricKeyHandle(key_data),
                                     blink::kWebCryptoKeyTypeSecret,
                                     extractable, algorithm, usages);
  return Status::Success();
}

void WriteSecretKeyJwk(const CryptoData& raw_key_data,
                       const std::string& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usages,
                       std::vector<uint8_t>* jwk_key_data) {
  JwkWriter writer(algorithm, extractable, usages, "oct");
  writer.SetBytes("k", raw_key_data);
  writer.ToJson(jwk_key_data);
}

Status ReadSecretKeyNoExpectedAlgJwk(
    const CryptoData& key_data,
    bool expected_extractable,
    blink::WebCryptoKeyUsageMask expected_usages,
    std::vector<uint8_t>* raw_key_data,
    JwkReader* jwk) {
  Status status = jwk->Init(key_data, expected_extractable, expected_usages,
                            "oct", std::string());
  if (status.IsError())
    return status;

  std::string jwk_k_value;
  status = jwk->GetBytes("k", &jwk_k_value);
  if (status.IsError())
    return status;
  raw_key_data->assign(jwk_k_value.begin(), jwk_k_value.end());

  return Status::Success();
}

}  // namespace webcrypto
