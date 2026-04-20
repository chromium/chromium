// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/crypto_private_key.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "crypto/sign.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

std::optional<crypto::sign::SignatureKind> MapAlgorithm(uint16_t algorithm) {
  switch (algorithm) {
    case SSL_SIGN_RSA_PKCS1_SHA1:
      return crypto::sign::RSA_PKCS1_SHA1;
    case SSL_SIGN_RSA_PKCS1_SHA256:
      return crypto::sign::RSA_PKCS1_SHA256;
    case SSL_SIGN_RSA_PKCS1_SHA384:
      return crypto::sign::RSA_PKCS1_SHA384;
    case SSL_SIGN_RSA_PKCS1_SHA512:
      return crypto::sign::RSA_PKCS1_SHA512;
    case SSL_SIGN_RSA_PSS_SHA256:
      return crypto::sign::RSA_PSS_SHA256;
    case SSL_SIGN_RSA_PSS_SHA384:
      return crypto::sign::RSA_PSS_SHA384;
    case SSL_SIGN_RSA_PSS_SHA512:
      return crypto::sign::RSA_PSS_SHA512;
    case SSL_SIGN_ECDSA_SHA1:
      return crypto::sign::ECDSA_SHA1;
    case SSL_SIGN_ECDSA_SECP256R1_SHA256:
      return crypto::sign::ECDSA_SHA256;
    case SSL_SIGN_ECDSA_SECP384R1_SHA384:
      return crypto::sign::ECDSA_SHA384;
    case SSL_SIGN_ECDSA_SECP521R1_SHA512:
      return crypto::sign::ECDSA_SHA512;
    case SSL_SIGN_ED25519:
      return crypto::sign::ED25519;
  }
  return std::nullopt;
}

class CryptoPrivateKey : public ThreadedSSLPrivateKey::Delegate {
 public:
  explicit CryptoPrivateKey(crypto::keypair::PrivateKey key)
      : key_(std::move(key)) {}

  CryptoPrivateKey(const CryptoPrivateKey&) = delete;
  CryptoPrivateKey& operator=(const CryptoPrivateKey&) = delete;

  ~CryptoPrivateKey() override = default;

  std::string GetProviderName() override {
    return "crypto::keypair::PrivateKey";
  }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    int id = EVP_PKEY_id(key_.key());
    // SSLPrivateKey doesn't support ED25519 signatures by default, but this
    // class does because crypto::sign::Sign() can handle them.
    if (id == EVP_PKEY_ED25519) {
      return {SSL_SIGN_ED25519};
    }

    return SSLPrivateKey::DefaultAlgorithmPreferences(EVP_PKEY_id(key_.key()),
                                                      true /* supports PSS */);
  }

  Error Sign(uint16_t algorithm,
             base::span<const uint8_t> input,
             std::vector<uint8_t>* signature) override {
    std::optional<crypto::sign::SignatureKind> kind = MapAlgorithm(algorithm);
    if (!kind) {
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }

    *signature = crypto::sign::Sign(*kind, key_, input);
    return OK;
  }

 private:
  crypto::keypair::PrivateKey key_;
};

}  // namespace

scoped_refptr<SSLPrivateKey> WrapCryptoPrivateKey(
    crypto::keypair::PrivateKey key) {
  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<CryptoPrivateKey>(std::move(key)),
      GetSSLPlatformKeyTaskRunner());
}

}  // namespace net
