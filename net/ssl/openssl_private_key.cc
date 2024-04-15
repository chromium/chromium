// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_private_key.h"

#include "base/check.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

class OpenSSLPrivateKey : public ThreadedSSLPrivateKey::Delegate {
 public:
  explicit OpenSSLPrivateKey(bssl::UniquePtr<EVP_PKEY> key)
      : key_(std::move(key)) {}

  OpenSSLPrivateKey(const OpenSSLPrivateKey&) = delete;
  OpenSSLPrivateKey& operator=(const OpenSSLPrivateKey&) = delete;

  ~OpenSSLPrivateKey() override = default;

  std::string GetProviderName() override { return "EVP_PKEY"; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return SSLPrivateKey::DefaultAlgorithmPreferences(EVP_PKEY_id(key_.get()),
                                                      true /* supports PSS */);
  }

  Error Sign(uint16_t algorithm,
             base::span<const uint8_t> input,
             std::vector<uint8_t>* signature) override {
    bssl::ScopedEVP_MD_CTX ctx;
    EVP_PKEY_CTX* pctx;
    if (!EVP_DigestSignInit(ctx.get(), &pctx,
                            SSL_get_signature_algorithm_digest(algorithm),
                            nullptr, key_.get())) {
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    if (SSL_is_signature_algorithm_rsa_pss(algorithm)) {
      if (!EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) ||
          !EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1 /* hash length */)) {
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
    }
    size_t sig_len = 0;
    if (!EVP_DigestSign(ctx.get(), nullptr, &sig_len, input.data(),
                        input.size())) {
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(sig_len);
    if (!EVP_DigestSign(ctx.get(), signature->data(), &sig_len, input.data(),
                        input.size())) {
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(sig_len);
    return OK;
  }

 private:
  bssl::UniquePtr<EVP_PKEY> key_;
};

}  // namespace

scoped_refptr<SSLPrivateKey> WrapOpenSSLPrivateKey(
    bssl::UniquePtr<EVP_PKEY> key) {
  CHECK(key);
  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<OpenSSLPrivateKey>(std::move(key)),
      GetSSLPlatformKeyTaskRunner());
}

}  // namespace net
