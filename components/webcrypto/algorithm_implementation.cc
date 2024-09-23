// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithm_implementation.h"

#include "base/notreached.h"
#include "components/webcrypto/algorithms/asymmetric_key_util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"

namespace webcrypto {

AlgorithmImplementation::~AlgorithmImplementation() {
}

Status AlgorithmImplementation::Encrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    base::span<const uint8_t> data,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::Decrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    base::span<const uint8_t> data,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::Sign(const blink::WebCryptoAlgorithm& algorithm,
                                     const blink::WebCryptoKey& key,
                                     base::span<const uint8_t> data,
                                     std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::Verify(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    base::span<const uint8_t> signature,
    base::span<const uint8_t> data,
    bool* signature_match) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::Digest(
    const blink::WebCryptoAlgorithm& algorithm,
    base::span<const uint8_t> data,
    std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    GenerateKeyResult* result) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::DeriveBits(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& base_key,
    std::optional<unsigned int> length_bits,
    std::vector<uint8_t>* derived_bytes) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::GetKeyLength(
    const blink::WebCryptoAlgorithm& key_length_algorithm,
    std::optional<unsigned int>* length_bits) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::ImportKey(
    blink::WebCryptoKeyFormat format,
    base::span<const uint8_t> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::ExportKey(blink::WebCryptoKeyFormat format,
                                          const blink::WebCryptoKey& key,
                                          std::vector<uint8_t>* buffer) const {
  return Status::ErrorUnsupported();
}

Status AlgorithmImplementation::SerializeKeyForClone(
    const blink::WebCryptoKey& key,
    blink::WebVector<uint8_t>* key_data) const {
  switch (key.GetType()) {
    case blink::kWebCryptoKeyTypeSecret:
      *key_data = GetSymmetricKeyData(key);
      return Status::Success();

    case blink::kWebCryptoKeyTypePublic: {
      std::vector<uint8_t> vec;
      Status status = ExportPKeySpki(GetEVP_PKEY(key), &vec);
      if (status.IsSuccess()) {
        *key_data = vec;
      }
      return status;
    }

    case blink::kWebCryptoKeyTypePrivate: {
      std::vector<uint8_t> vec;
      Status status = ExportPKeyPkcs8(GetEVP_PKEY(key), &vec);
      if (status.IsSuccess()) {
        *key_data = vec;
      }
      return status;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return Status::ErrorUnexpected();
}

Status AlgorithmImplementation::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    base::span<const uint8_t> key_data,
    blink::WebCryptoKey* key) const {
  return Status::ErrorUnsupported();
}

}  // namespace webcrypto
