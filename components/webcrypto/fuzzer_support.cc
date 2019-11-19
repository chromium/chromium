// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/fuzzer_support.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "mojo/core/embedder/embedder.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/web/blink.h"

namespace webcrypto {

namespace {

// This mock is used to initialize blink.
class InitOnce : public blink::Platform {
 public:
  InitOnce() {
    base::CommandLine::Init(0, nullptr);
    mojo::core::Init();
    blink::Platform::CreateMainThreadAndInitialize(this);
  }
  ~InitOnce() override {}

 private:
  base::SingleThreadTaskExecutor main_thread_task_executor_;
};

base::LazyInstance<InitOnce>::Leaky g_once = LAZY_INSTANCE_INITIALIZER;

void EnsureInitialized() {
  g_once.Get();
}

blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(blink::WebCryptoAlgorithm::IsHash(hash_id));
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      id,
      new blink::WebCryptoRsaHashedImportParams(
          blink::WebCryptoAlgorithm::AdoptParamsAndCreate(hash_id, nullptr)));
}

blink::WebCryptoAlgorithm CreateEcImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoNamedCurve named_curve) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      id, new blink::WebCryptoEcKeyImportParams(named_curve));
}

}  // namespace

blink::WebCryptoKeyUsageMask GetCompatibleKeyUsages(
    blink::WebCryptoKeyFormat format) {
  // SPKI format implies import of a public key, whereas PKCS8 implies import
  // of a private key. Pick usages that are compatible with a signature
  // algorithm.
  return format == blink::kWebCryptoKeyFormatSpki
             ? blink::kWebCryptoKeyUsageVerify
             : blink::kWebCryptoKeyUsageSign;
}

void ImportEcKeyFromDerFuzzData(const uint8_t* data,
                                size_t size,
                                blink::WebCryptoKeyFormat format) {
  DCHECK(format == blink::kWebCryptoKeyFormatSpki ||
         format == blink::kWebCryptoKeyFormatPkcs8);
  EnsureInitialized();

  // There are 3 possible EC named curves. Fix this parameter. It shouldn't
  // matter based on the current implementation for PKCS8 or SPKI. But it
  // will have an impact when parsing JWK format.
  blink::WebCryptoNamedCurve curve = blink::kWebCryptoNamedCurveP384;

  // Always use ECDSA as the algorithm. Shouldn't make much difference for
  // non-JWK formats.
  blink::WebCryptoAlgorithmId algorithm_id = blink::kWebCryptoAlgorithmIdEcdsa;

  // Use key usages that are compatible with the chosen algorithm and key type.
  blink::WebCryptoKeyUsageMask usages = GetCompatibleKeyUsages(format);

  blink::WebCryptoKey key;
  webcrypto::Status status = webcrypto::ImportKey(
      format, webcrypto::CryptoData(data, base::checked_cast<uint32_t>(size)),
      CreateEcImportAlgorithm(algorithm_id, curve), true, usages, &key);

  // These errors imply a bad setup of parameters, and means ImportKey() may not
  // be testing the actual parsing.
  DCHECK_NE(status.error_details(),
            Status::ErrorUnsupportedImportKeyFormat().error_details());
  DCHECK_NE(status.error_details(),
            Status::ErrorCreateKeyBadUsages().error_details());
}

void ImportEcKeyFromRawFuzzData(const uint8_t* data, size_t size) {
  EnsureInitialized();

  // There are 3 possible EC named curves. Consume the first byte to decide on
  // the curve.
  uint8_t curve_index = 0;
  if (size > 0) {
    curve_index = data[0];
    data++;
    size--;
  }

  blink::WebCryptoNamedCurve curve;

  switch (curve_index % 3) {
    case 0:
      curve = blink::kWebCryptoNamedCurveP256;
      break;
    case 1:
      curve = blink::kWebCryptoNamedCurveP384;
      break;
    default:
      curve = blink::kWebCryptoNamedCurveP521;
      break;
  }

  // Always use ECDSA as the algorithm. Shouldn't make an difference for import.
  blink::WebCryptoAlgorithmId algorithm_id = blink::kWebCryptoAlgorithmIdEcdsa;

  // Use key usages that are compatible with the chosen algorithm and key type.
  blink::WebCryptoKeyUsageMask usages = blink::kWebCryptoKeyUsageVerify;

  blink::WebCryptoKey key;
  webcrypto::Status status = webcrypto::ImportKey(
      blink::kWebCryptoKeyFormatRaw,
      webcrypto::CryptoData(data, base::checked_cast<uint32_t>(size)),
      CreateEcImportAlgorithm(algorithm_id, curve), true, usages, &key);

  // These errors imply a bad setup of parameters, and means ImportKey() may not
  // be testing the actual parsing.
  DCHECK_NE(status.error_details(),
            Status::ErrorUnsupportedImportKeyFormat().error_details());
  DCHECK_NE(status.error_details(),
            Status::ErrorCreateKeyBadUsages().error_details());
}

void ImportRsaKeyFromDerFuzzData(const uint8_t* data,
                                 size_t size,
                                 blink::WebCryptoKeyFormat format) {
  DCHECK(format == blink::kWebCryptoKeyFormatSpki ||
         format == blink::kWebCryptoKeyFormatPkcs8);
  EnsureInitialized();

  // There are several possible hash functions. Fix this parameter. It shouldn't
  // matter based on the current implementation for PKCS8 or SPKI. But it
  // will have an impact when parsing JWK format.
  blink::WebCryptoAlgorithmId hash_id = blink::kWebCryptoAlgorithmIdSha256;

  // Always use RSA-SSA PKCS#1 as the algorithm. Shouldn't make much difference
  // for non-JWK formats.
  blink::WebCryptoAlgorithmId algorithm_id =
      blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5;

  // Use key usages that are compatible with the chosen algorithm and key type.
  blink::WebCryptoKeyUsageMask usages = GetCompatibleKeyUsages(format);

  blink::WebCryptoKey key;
  webcrypto::Status status = webcrypto::ImportKey(
      format, webcrypto::CryptoData(data, base::checked_cast<uint32_t>(size)),
      CreateRsaHashedImportAlgorithm(algorithm_id, hash_id), true, usages,
      &key);

  // These errors imply a bad setup of parameters, and means ImportKey() may not
  // be testing the actual parsing.
  DCHECK_NE(status.error_details(),
            Status::ErrorUnsupportedImportKeyFormat().error_details());
  DCHECK_NE(status.error_details(),
            Status::ErrorCreateKeyBadUsages().error_details());
}

}  // namespace webcrypto
