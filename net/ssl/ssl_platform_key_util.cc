// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_util.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "crypto/openssl_util.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {

namespace {

class SSLPlatformKeyTaskRunner {
 public:
  SSLPlatformKeyTaskRunner() : worker_thread_("Platform Key Thread") {
    base::Thread::Options options;
    options.joinable = false;
    worker_thread_.StartWithOptions(options);
  }

  ~SSLPlatformKeyTaskRunner() = default;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return worker_thread_.task_runner();
  }

 private:
  base::Thread worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(SSLPlatformKeyTaskRunner);
};

base::LazyInstance<SSLPlatformKeyTaskRunner>::Leaky g_platform_key_task_runner =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

scoped_refptr<base::SingleThreadTaskRunner> GetSSLPlatformKeyTaskRunner() {
  return g_platform_key_task_runner.Get().task_runner();
}

bssl::UniquePtr<EVP_PKEY> GetClientCertPublicKey(
    const X509Certificate* certificate) {
  crypto::OpenSSLErrStackTracer tracker(FROM_HERE);

  base::StringPiece spki;
  if (!asn1::ExtractSPKIFromDERCert(
          x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki)) {
    LOG(ERROR) << "Could not extract SPKI from certificate.";
    return nullptr;
  }

  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> key(EVP_parse_public_key(&cbs));
  if (!key || CBS_len(&cbs) != 0) {
    LOG(ERROR) << "Could not parse public key.";
    return nullptr;
  }

  return key;
}

bool GetClientCertInfo(const X509Certificate* certificate,
                       int* out_type,
                       size_t* out_max_length) {
  bssl::UniquePtr<EVP_PKEY> key = GetClientCertPublicKey(certificate);
  if (!key) {
    return false;
  }

  *out_type = EVP_PKEY_id(key.get());
  *out_max_length = EVP_PKEY_size(key.get());
  return true;
}

base::Optional<std::vector<uint8_t>> AddPSSPadding(
    EVP_PKEY* pubkey,
    const EVP_MD* md,
    base::span<const uint8_t> digest) {
  RSA* rsa = EVP_PKEY_get0_RSA(pubkey);
  if (!rsa) {
    return base::nullopt;
  }
  std::vector<uint8_t> ret(RSA_size(rsa));
  if (digest.size() != EVP_MD_size(md) ||
      !RSA_padding_add_PKCS1_PSS_mgf1(rsa, ret.data(), digest.data(), md, md,
                                      -1 /* salt length is digest length */)) {
    return base::nullopt;
  }
  return ret;
}

}  // namespace net
