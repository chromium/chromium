// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/util.h"

#include "base/check_op.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/digest.h"

namespace webcrypto {

const EVP_MD* GetDigest(const blink::WebCryptoAlgorithm& hash_algorithm) {
  return GetDigest(hash_algorithm.Id());
}

const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::kWebCryptoAlgorithmIdSha1:
      return EVP_sha1();
    case blink::kWebCryptoAlgorithmIdSha256:
      return EVP_sha256();
    case blink::kWebCryptoAlgorithmIdSha384:
      return EVP_sha384();
    case blink::kWebCryptoAlgorithmIdSha512:
      return EVP_sha512();
    default:
      return nullptr;
  }
}

void TruncateToBitLength(size_t length_bits, std::vector<uint8_t>* bytes) {
  size_t length_bytes = NumBitsToBytes(length_bits);

  if (bytes->size() != length_bytes) {
    CHECK_LT(length_bytes, bytes->size());
    bytes->resize(length_bytes);
  }

  size_t remainder_bits = length_bits % 8;

  // Zero any "unused bits" in the final byte.
  if (remainder_bits)
    bytes->back() &= ~((0xFF) >> remainder_bits);
}

Status CheckKeyCreationUsages(blink::WebCryptoKeyUsageMask all_possible_usages,
                              blink::WebCryptoKeyUsageMask actual_usages) {
  if (!ContainsKeyUsages(all_possible_usages, actual_usages))
    return Status::ErrorCreateKeyBadUsages();
  return Status::Success();
}

bool ContainsKeyUsages(blink::WebCryptoKeyUsageMask a,
                       blink::WebCryptoKeyUsageMask b) {
  return (a & b) == b;
}

Status AeadEncryptDecrypt(EncryptOrDecrypt mode,
                          base::span<const uint8_t> raw_key,
                          base::span<const uint8_t> data,
                          unsigned int tag_length_bytes,
                          base::span<const uint8_t> iv,
                          base::span<const uint8_t> additional_data,
                          const EVP_AEAD* aead_alg,
                          std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_AEAD_CTX ctx;

  if (!aead_alg)
    return Status::ErrorUnexpected();

  if (!EVP_AEAD_CTX_init(ctx.get(), aead_alg, raw_key.data(), raw_key.size(),
                         tag_length_bytes, nullptr)) {
    return Status::OperationError();
  }

  size_t len;
  int ok;

  if (mode == DECRYPT) {
    if (data.size() < tag_length_bytes)
      return Status::ErrorDataTooSmall();

    buffer->resize(data.size() - tag_length_bytes);

    ok = EVP_AEAD_CTX_open(ctx.get(), buffer->data(), &len, buffer->size(),
                           iv.data(), iv.size(), data.data(), data.size(),
                           additional_data.data(), additional_data.size());
  } else {
    // No need to check for unsigned integer overflow here (seal fails if
    // the output buffer is too small).
    buffer->resize(data.size() + EVP_AEAD_max_overhead(aead_alg));

    ok = EVP_AEAD_CTX_seal(ctx.get(), buffer->data(), &len, buffer->size(),
                           iv.data(), iv.size(), data.data(), data.size(),
                           additional_data.data(), additional_data.size());
  }

  if (!ok)
    return Status::OperationError();
  buffer->resize(len);
  return Status::Success();
}

}  // namespace webcrypto
