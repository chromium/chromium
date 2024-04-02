// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_util.h"

#include <string_view>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
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
    worker_thread_.StartWithOptions(std::move(options));
  }

  SSLPlatformKeyTaskRunner(const SSLPlatformKeyTaskRunner&) = delete;
  SSLPlatformKeyTaskRunner& operator=(const SSLPlatformKeyTaskRunner&) = delete;

  ~SSLPlatformKeyTaskRunner() = default;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return worker_thread_.task_runner();
  }

 private:
  base::Thread worker_thread_;
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

  std::string_view spki;
  if (!asn1::ExtractSPKIFromDERCert(
          x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki)) {
    LOG(ERROR) << "Could not extract SPKI from certificate.";
    return nullptr;
  }

  return ParseSpki(base::as_byte_span(spki));
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

bssl::UniquePtr<EVP_PKEY> ParseSpki(base::span<const uint8_t> spki) {
  CBS cbs;
  CBS_init(&cbs, spki.data(), spki.size());
  bssl::UniquePtr<EVP_PKEY> key(EVP_parse_public_key(&cbs));
  if (!key || CBS_len(&cbs) != 0) {
    LOG(ERROR) << "Could not parse public key.";
    return nullptr;
  }
  return key;
}

bool GetPublicKeyInfo(base::span<const uint8_t> spki,
                      int* out_type,
                      size_t* out_max_length) {
  auto key = ParseSpki(spki);
  if (!key) {
    return false;
  }

  *out_type = EVP_PKEY_id(key.get());
  *out_max_length = EVP_PKEY_size(key.get());
  return true;
}

std::optional<std::vector<uint8_t>> AddPSSPadding(
    EVP_PKEY* pubkey,
    const EVP_MD* md,
    base::span<const uint8_t> digest) {
  RSA* rsa = EVP_PKEY_get0_RSA(pubkey);
  if (!rsa) {
    return std::nullopt;
  }
  std::vector<uint8_t> ret(RSA_size(rsa));
  if (digest.size() != EVP_MD_size(md) ||
      !RSA_padding_add_PKCS1_PSS_mgf1(rsa, ret.data(), digest.data(), md, md,
                                      -1 /* salt length is digest length */)) {
    return std::nullopt;
  }
  return ret;
}

}  // namespace net
