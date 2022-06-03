// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithm_registry.h"

#include "base/lazy_instance.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithm_implementations.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"

namespace webcrypto {

namespace {

// This class is used as a singleton. All methods must be threadsafe.
class AlgorithmRegistry {
 public:
  AlgorithmRegistry()
      : sha_(CreateShaImplementation()),
        aes_gcm_(CreateAesGcmImplementation()),
        aes_cbc_(CreateAesCbcImplementation()),
        aes_ctr_(CreateAesCtrImplementation()),
        aes_kw_(CreateAesKwImplementation()),
        hmac_(CreateHmacImplementation()),
        rsa_ssa_(CreateRsaSsaImplementation()),
        rsa_oaep_(CreateRsaOaepImplementation()),
        rsa_pss_(CreateRsaPssImplementation()),
        ecdsa_(CreateEcdsaImplementation()),
        ecdh_(CreateEcdhImplementation()),
        hkdf_(CreateHkdfImplementation()),
        pbkdf2_(CreatePbkdf2Implementation()),
        ed25519_(CreateEd25519Implementation()),
        x25519_(CreateX25519Implementation()) {
    crypto::EnsureOpenSSLInit();
  }

  const AlgorithmImplementation* GetAlgorithm(
      blink::WebCryptoAlgorithmId id) const {
    switch (id) {
      case blink::kWebCryptoAlgorithmIdSha1:
      case blink::kWebCryptoAlgorithmIdSha256:
      case blink::kWebCryptoAlgorithmIdSha384:
      case blink::kWebCryptoAlgorithmIdSha512:
        return sha_.get();
      case blink::kWebCryptoAlgorithmIdAesGcm:
        return aes_gcm_.get();
      case blink::kWebCryptoAlgorithmIdAesCbc:
        return aes_cbc_.get();
      case blink::kWebCryptoAlgorithmIdAesCtr:
        return aes_ctr_.get();
      case blink::kWebCryptoAlgorithmIdAesKw:
        return aes_kw_.get();
      case blink::kWebCryptoAlgorithmIdHmac:
        return hmac_.get();
      case blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
        return rsa_ssa_.get();
      case blink::kWebCryptoAlgorithmIdRsaOaep:
        return rsa_oaep_.get();
      case blink::kWebCryptoAlgorithmIdRsaPss:
        return rsa_pss_.get();
      case blink::kWebCryptoAlgorithmIdEcdsa:
        return ecdsa_.get();
      case blink::kWebCryptoAlgorithmIdEcdh:
        return ecdh_.get();
      case blink::kWebCryptoAlgorithmIdHkdf:
        return hkdf_.get();
      case blink::kWebCryptoAlgorithmIdPbkdf2:
        return pbkdf2_.get();
      case blink::kWebCryptoAlgorithmIdEd25519:
        return ed25519_.get();
      case blink::kWebCryptoAlgorithmIdX25519:
        return x25519_.get();
      default:
        return nullptr;
    }
  }

 private:
  const std::unique_ptr<AlgorithmImplementation> sha_;
  const std::unique_ptr<AlgorithmImplementation> aes_gcm_;
  const std::unique_ptr<AlgorithmImplementation> aes_cbc_;
  const std::unique_ptr<AlgorithmImplementation> aes_ctr_;
  const std::unique_ptr<AlgorithmImplementation> aes_kw_;
  const std::unique_ptr<AlgorithmImplementation> hmac_;
  const std::unique_ptr<AlgorithmImplementation> rsa_ssa_;
  const std::unique_ptr<AlgorithmImplementation> rsa_oaep_;
  const std::unique_ptr<AlgorithmImplementation> rsa_pss_;
  const std::unique_ptr<AlgorithmImplementation> ecdsa_;
  const std::unique_ptr<AlgorithmImplementation> ecdh_;
  const std::unique_ptr<AlgorithmImplementation> hkdf_;
  const std::unique_ptr<AlgorithmImplementation> pbkdf2_;
  const std::unique_ptr<AlgorithmImplementation> ed25519_;
  const std::unique_ptr<AlgorithmImplementation> x25519_;
};

}  // namespace

base::LazyInstance<AlgorithmRegistry>::Leaky g_algorithm_registry =
    LAZY_INSTANCE_INITIALIZER;

Status GetAlgorithmImplementation(blink::WebCryptoAlgorithmId id,
                                  const AlgorithmImplementation** impl) {
  *impl = g_algorithm_registry.Get().GetAlgorithm(id);
  if (*impl)
    return Status::Success();
  return Status::ErrorUnsupported();
}

}  // namespace webcrypto
