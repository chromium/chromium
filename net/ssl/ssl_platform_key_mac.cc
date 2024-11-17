// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ssl/ssl_platform_key_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecKey.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_policy.h"
#include "base/numerics/safe_conversions.h"
#include "crypto/openssl_util.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_apple.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

// Returns the corresponding SecKeyAlgorithm or nullptr if unrecognized.
SecKeyAlgorithm GetSecKeyAlgorithm(uint16_t algorithm) {
  switch (algorithm) {
    case SSL_SIGN_RSA_PKCS1_SHA512:
      return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512;
    case SSL_SIGN_RSA_PKCS1_SHA384:
      return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384;
    case SSL_SIGN_RSA_PKCS1_SHA256:
      return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256;
    case SSL_SIGN_RSA_PKCS1_SHA1:
      return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1;
    case SSL_SIGN_ECDSA_SECP521R1_SHA512:
      return kSecKeyAlgorithmECDSASignatureDigestX962SHA512;
    case SSL_SIGN_ECDSA_SECP384R1_SHA384:
      return kSecKeyAlgorithmECDSASignatureDigestX962SHA384;
    case SSL_SIGN_ECDSA_SECP256R1_SHA256:
      return kSecKeyAlgorithmECDSASignatureDigestX962SHA256;
    case SSL_SIGN_ECDSA_SHA1:
      return kSecKeyAlgorithmECDSASignatureDigestX962SHA1;
    case SSL_SIGN_RSA_PSS_SHA512:
      return kSecKeyAlgorithmRSASignatureDigestPSSSHA512;
    case SSL_SIGN_RSA_PSS_SHA384:
      return kSecKeyAlgorithmRSASignatureDigestPSSSHA384;
    case SSL_SIGN_RSA_PSS_SHA256:
      return kSecKeyAlgorithmRSASignatureDigestPSSSHA256;
  }

  return nullptr;
}

class SSLPlatformKeySecKey : public ThreadedSSLPrivateKey::Delegate {
 public:
  SSLPlatformKeySecKey(bssl::UniquePtr<EVP_PKEY> pubkey, SecKeyRef key)
      : pubkey_(std::move(pubkey)), key_(key, base::scoped_policy::RETAIN) {
    // Determine the algorithms supported by the key.
    for (uint16_t algorithm : SSLPrivateKey::DefaultAlgorithmPreferences(
             EVP_PKEY_id(pubkey_.get()), true /* include PSS */)) {
      bool unused;
      if (GetSecKeyAlgorithmWithFallback(algorithm, &unused)) {
        preferences_.push_back(algorithm);
      }
    }
  }

  SSLPlatformKeySecKey(const SSLPlatformKeySecKey&) = delete;
  SSLPlatformKeySecKey& operator=(const SSLPlatformKeySecKey&) = delete;

  ~SSLPlatformKeySecKey() override = default;

  std::string GetProviderName() override {
    // TODO(crbug.com/41423739): Is there a more descriptive name to
    // return?
    return "SecKey";
  }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return preferences_;
  }

  Error Sign(uint16_t algorithm,
             base::span<const uint8_t> input,
             std::vector<uint8_t>* signature) override {
    bool pss_fallback = false;
    SecKeyAlgorithm sec_algorithm =
        GetSecKeyAlgorithmWithFallback(algorithm, &pss_fallback);
    if (!sec_algorithm) {
      // The caller should not request a signature algorithm we do not support.
      // However, it's possible `key_` previously reported it supported an
      // algorithm but no longer does. A compromised network service could also
      // request invalid algorithms, so cleanly fail.
      LOG(ERROR) << "Unsupported signature algorithm: " << algorithm;
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }

    const EVP_MD* md = SSL_get_signature_algorithm_digest(algorithm);
    uint8_t digest_buf[EVP_MAX_MD_SIZE];
    unsigned digest_len;
    if (!md || !EVP_Digest(input.data(), input.size(), digest_buf, &digest_len,
                           md, nullptr)) {
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    base::span<const uint8_t> digest = base::make_span(digest_buf, digest_len);

    std::optional<std::vector<uint8_t>> pss_storage;
    if (pss_fallback) {
      // Implement RSA-PSS by adding the padding manually and then using
      // kSecKeyAlgorithmRSASignatureRaw.
      DCHECK(SSL_is_signature_algorithm_rsa_pss(algorithm));
      DCHECK_EQ(sec_algorithm, kSecKeyAlgorithmRSASignatureRaw);
      pss_storage = AddPSSPadding(pubkey_.get(), md, digest);
      if (!pss_storage) {
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
      digest = *pss_storage;
    }

    base::apple::ScopedCFTypeRef<CFDataRef> digest_ref(
        CFDataCreate(kCFAllocatorDefault, digest.data(),
                     base::checked_cast<CFIndex>(digest.size())));

    base::apple::ScopedCFTypeRef<CFErrorRef> error;
    base::apple::ScopedCFTypeRef<CFDataRef> signature_ref(SecKeyCreateSignature(
        key_.get(), sec_algorithm, digest_ref.get(), error.InitializeInto()));
    if (!signature_ref) {
      LOG(ERROR) << error.get();
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }

    auto signature_span = base::apple::CFDataToSpan(signature_ref.get());
    signature->assign(signature_span.begin(), signature_span.end());
    return OK;
  }

 private:
  // Returns the algorithm to use with |algorithm| and this key, or nullptr if
  // not supported. If the resulting algorithm should be manually padded for
  // RSA-PSS, |*out_pss_fallback| is set to true.
  SecKeyAlgorithm GetSecKeyAlgorithmWithFallback(uint16_t algorithm,
                                                 bool* out_pss_fallback) {
    SecKeyAlgorithm sec_algorithm = GetSecKeyAlgorithm(algorithm);
    if (sec_algorithm &&
        SecKeyIsAlgorithmSupported(key_.get(), kSecKeyOperationTypeSign,
                                   sec_algorithm)) {
      *out_pss_fallback = false;
      return sec_algorithm;
    }

    if (SSL_is_signature_algorithm_rsa_pss(algorithm) &&
        SecKeyIsAlgorithmSupported(key_.get(), kSecKeyOperationTypeSign,
                                   kSecKeyAlgorithmRSASignatureRaw)) {
      *out_pss_fallback = true;
      return kSecKeyAlgorithmRSASignatureRaw;
    }

    return nullptr;
  }

  std::vector<uint16_t> preferences_;
  bssl::UniquePtr<EVP_PKEY> pubkey_;
  base::apple::ScopedCFTypeRef<SecKeyRef> key_;
};

}  // namespace

scoped_refptr<SSLPrivateKey> CreateSSLPrivateKeyForSecKey(
    const X509Certificate* certificate,
    SecKeyRef key) {
  bssl::UniquePtr<EVP_PKEY> pubkey = GetClientCertPublicKey(certificate);
  if (!pubkey)
    return nullptr;

  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<SSLPlatformKeySecKey>(std::move(pubkey), key),
      GetSSLPlatformKeyTaskRunner());
}

scoped_refptr<SSLPrivateKey> WrapUnexportableKey(
    const crypto::UnexportableSigningKey& unexportable_key) {
  bssl::UniquePtr<EVP_PKEY> pubkey =
      ParseSpki(unexportable_key.GetSubjectPublicKeyInfo());
  if (!pubkey) {
    return nullptr;
  }

  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<SSLPlatformKeySecKey>(std::move(pubkey),
                                             unexportable_key.GetSecKeyRef()),
      GetSSLPlatformKeyTaskRunner());
}

}  // namespace net
