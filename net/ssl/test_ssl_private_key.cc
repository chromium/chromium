// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/test_ssl_private_key.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/task/sequenced_task_runner.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/ssl/openssl_private_key.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {

namespace {

class FailingSSLPlatformKey : public ThreadedSSLPrivateKey::Delegate {
 public:
  FailingSSLPlatformKey() = default;

  FailingSSLPlatformKey(const FailingSSLPlatformKey&) = delete;
  FailingSSLPlatformKey& operator=(const FailingSSLPlatformKey&) = delete;

  ~FailingSSLPlatformKey() override = default;

  std::string GetProviderName() override { return "FailingSSLPlatformKey"; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return SSLPrivateKey::DefaultAlgorithmPreferences(EVP_PKEY_RSA,
                                                      true /* supports PSS */);
  }

  Error Sign(uint16_t algorithm,
             base::span<const uint8_t> input,
             std::vector<uint8_t>* signature) override {
    return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
  }
};

class SSLPrivateKeyWithPreferences : public SSLPrivateKey {
 public:
  SSLPrivateKeyWithPreferences(scoped_refptr<SSLPrivateKey> key,
                               base::span<const uint16_t> prefs)
      : key_(std::move(key)), prefs_(base::ToVector(prefs)) {}

  std::string GetProviderName() override { return key_->GetProviderName(); }

  std::vector<uint16_t> GetAlgorithmPreferences() override { return prefs_; }

  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override {
    if (!base::Contains(prefs_, algorithm)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
                                    std::vector<uint8_t>()));
      return;
    }

    key_->Sign(algorithm, input, std::move(callback));
  }

 private:
  friend class base::RefCountedThreadSafe<SSLPrivateKeyWithPreferences>;
  ~SSLPrivateKeyWithPreferences() override = default;

  scoped_refptr<SSLPrivateKey> key_;
  std::vector<uint16_t> prefs_;
};

}  // namespace

scoped_refptr<SSLPrivateKey> WrapRSAPrivateKey(
    crypto::RSAPrivateKey* rsa_private_key) {
  return net::WrapOpenSSLPrivateKey(bssl::UpRef(rsa_private_key->key()));
}

scoped_refptr<SSLPrivateKey> CreateFailSigningSSLPrivateKey() {
  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<FailingSSLPlatformKey>(), GetSSLPlatformKeyTaskRunner());
}

scoped_refptr<SSLPrivateKey> WrapSSLPrivateKeyWithPreferences(
    scoped_refptr<SSLPrivateKey> key,
    base::span<const uint16_t> prefs) {
  return base::MakeRefCounted<SSLPrivateKeyWithPreferences>(std::move(key),
                                                            prefs);
}

}  // namespace net
