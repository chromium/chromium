// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/crypto/crypto_histograms.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

static WebFeature AlgorithmIdToFeature(WebCryptoAlgorithmId id) {
  switch (id) {
    case kWebCryptoAlgorithmIdAesCbc:
      return WebFeature::kCryptoAlgorithmAesCbc;
    case kWebCryptoAlgorithmIdHmac:
      return WebFeature::kCryptoAlgorithmHmac;
    case kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return WebFeature::kCryptoAlgorithmRsaSsaPkcs1v1_5;
    case kWebCryptoAlgorithmIdSha1:
      return WebFeature::kCryptoAlgorithmSha1;
    case kWebCryptoAlgorithmIdSha256:
      return WebFeature::kCryptoAlgorithmSha256;
    case kWebCryptoAlgorithmIdSha384:
      return WebFeature::kCryptoAlgorithmSha384;
    case kWebCryptoAlgorithmIdSha512:
      return WebFeature::kCryptoAlgorithmSha512;
    case kWebCryptoAlgorithmIdAesGcm:
      return WebFeature::kCryptoAlgorithmAesGcm;
    case kWebCryptoAlgorithmIdRsaOaep:
      return WebFeature::kCryptoAlgorithmRsaOaep;
    case kWebCryptoAlgorithmIdAesCtr:
      return WebFeature::kCryptoAlgorithmAesCtr;
    case kWebCryptoAlgorithmIdAesKw:
      return WebFeature::kCryptoAlgorithmAesKw;
    case kWebCryptoAlgorithmIdRsaPss:
      return WebFeature::kCryptoAlgorithmRsaPss;
    case kWebCryptoAlgorithmIdEcdsa:
      return WebFeature::kCryptoAlgorithmEcdsa;
    case kWebCryptoAlgorithmIdEcdh:
      return WebFeature::kCryptoAlgorithmEcdh;
    case kWebCryptoAlgorithmIdHkdf:
      return WebFeature::kCryptoAlgorithmHkdf;
    case kWebCryptoAlgorithmIdPbkdf2:
      return WebFeature::kCryptoAlgorithmPbkdf2;
  }

  NOTREACHED();
  return static_cast<WebFeature>(0);
}

static void HistogramAlgorithmId(ExecutionContext* context,
                                 WebCryptoAlgorithmId algorithm_id) {
  WebFeature feature = AlgorithmIdToFeature(algorithm_id);
  if (static_cast<bool>(feature))
    UseCounter::Count(context, feature);
}

void HistogramAlgorithm(ExecutionContext* context,
                        const WebCryptoAlgorithm& algorithm) {
  HistogramAlgorithmId(context, algorithm.Id());

  // Histogram any interesting parameters for the algorithm. For instance
  // the inner hash for algorithms which include one (HMAC, RSA-PSS, etc)
  switch (algorithm.ParamsType()) {
    case kWebCryptoAlgorithmParamsTypeHmacImportParams:
      HistogramAlgorithm(context, algorithm.HmacImportParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeHmacKeyGenParams:
      HistogramAlgorithm(context, algorithm.HmacKeyGenParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
      HistogramAlgorithm(context, algorithm.RsaHashedKeyGenParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeRsaHashedImportParams:
      HistogramAlgorithm(context, algorithm.RsaHashedImportParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeEcdsaParams:
      HistogramAlgorithm(context, algorithm.EcdsaParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeHkdfParams:
      HistogramAlgorithm(context, algorithm.HkdfParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypePbkdf2Params:
      HistogramAlgorithm(context, algorithm.Pbkdf2Params()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeEcdhKeyDeriveParams:
    case kWebCryptoAlgorithmParamsTypeNone:
    case kWebCryptoAlgorithmParamsTypeAesCbcParams:
    case kWebCryptoAlgorithmParamsTypeAesGcmParams:
    case kWebCryptoAlgorithmParamsTypeAesKeyGenParams:
    case kWebCryptoAlgorithmParamsTypeRsaOaepParams:
    case kWebCryptoAlgorithmParamsTypeAesCtrParams:
    case kWebCryptoAlgorithmParamsTypeRsaPssParams:
    case kWebCryptoAlgorithmParamsTypeEcKeyGenParams:
    case kWebCryptoAlgorithmParamsTypeEcKeyImportParams:
    case kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams:
      break;
  }
}

void HistogramKey(ExecutionContext* context, const WebCryptoKey& key) {
  const WebCryptoKeyAlgorithm& algorithm = key.Algorithm();

  HistogramAlgorithmId(context, algorithm.Id());

  // Histogram any interesting parameters that are attached to the key. For
  // instance the inner hash being used for HMAC.
  switch (algorithm.ParamsType()) {
    case kWebCryptoKeyAlgorithmParamsTypeHmac:
      HistogramAlgorithm(context, algorithm.HmacParams()->GetHash());
      break;
    case kWebCryptoKeyAlgorithmParamsTypeRsaHashed:
      HistogramAlgorithm(context, algorithm.RsaHashedParams()->GetHash());
      break;
    case kWebCryptoKeyAlgorithmParamsTypeNone:
    case kWebCryptoKeyAlgorithmParamsTypeAes:
    case kWebCryptoKeyAlgorithmParamsTypeEc:
      break;
  }
}

void HistogramAlgorithmAndKey(ExecutionContext* context,
                              const WebCryptoAlgorithm& algorithm,
                              const WebCryptoKey& key) {
  // Note that the algorithm ID for |algorithm| and |key| will usually be the
  // same. This is OK because UseCounter only increments things once per the
  // context.
  HistogramAlgorithm(context, algorithm);
  HistogramKey(context, key);
}

}  // namespace blink
