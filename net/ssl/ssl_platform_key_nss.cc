// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ssl/ssl_platform_key_nss.h"

#include <cert.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <prerror.h>
#include <secmodt.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

void LogPRError(const char* message) {
  PRErrorCode err = PR_GetError();
  const char* err_name = PR_ErrorToName(err);
  if (err_name == nullptr)
    err_name = "";
  LOG(ERROR) << message << ": " << err << " (" << err_name << ")";
}

class SSLPlatformKeyNSS : public ThreadedSSLPrivateKey::Delegate {
 public:
  SSLPlatformKeyNSS(int type,
                    scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
                        password_delegate,
                    crypto::ScopedSECKEYPrivateKey key)
      : type_(type),
        password_delegate_(std::move(password_delegate)),
        key_(std::move(key)),
        supports_pss_(PK11_DoesMechanism(key_->pkcs11Slot, CKM_RSA_PKCS_PSS)) {}

  SSLPlatformKeyNSS(const SSLPlatformKeyNSS&) = delete;
  SSLPlatformKeyNSS& operator=(const SSLPlatformKeyNSS&) = delete;

  ~SSLPlatformKeyNSS() override = default;

  std::string GetProviderName() override {
    // This logic accesses fields directly on the struct, so it may run on any
    // thread without caching.
    return base::StringPrintf("%s, %s",
                              PK11_GetModule(key_->pkcs11Slot)->commonName,
                              PK11_GetSlotName(key_->pkcs11Slot));
  }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return SSLPrivateKey::DefaultAlgorithmPreferences(type_, supports_pss_);
  }

  Error Sign(uint16_t algorithm,
             base::span<const uint8_t> input,
             std::vector<uint8_t>* signature) override {
    const EVP_MD* md = SSL_get_signature_algorithm_digest(algorithm);
    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned digest_len;
    if (!md || !EVP_Digest(input.data(), input.size(), digest, &digest_len, md,
                           nullptr)) {
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    SECItem digest_item;
    digest_item.data = digest;
    digest_item.len = digest_len;

    CK_MECHANISM_TYPE mechanism = PK11_MapSignKeyType(key_->keyType);
    SECItem param = {siBuffer, nullptr, 0};
    CK_RSA_PKCS_PSS_PARAMS pss_params;
    bssl::UniquePtr<uint8_t> free_digest_info;
    if (SSL_is_signature_algorithm_rsa_pss(algorithm)) {
      switch (EVP_MD_type(md)) {
        case NID_sha256:
          pss_params.hashAlg = CKM_SHA256;
          pss_params.mgf = CKG_MGF1_SHA256;
          break;
        case NID_sha384:
          pss_params.hashAlg = CKM_SHA384;
          pss_params.mgf = CKG_MGF1_SHA384;
          break;
        case NID_sha512:
          pss_params.hashAlg = CKM_SHA512;
          pss_params.mgf = CKG_MGF1_SHA512;
          break;
        default:
          LOG(ERROR) << "Unexpected hash algorithm";
          return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
      // Use the hash length for the salt length.
      pss_params.sLen = EVP_MD_size(md);
      mechanism = CKM_RSA_PKCS_PSS;
      param.data = reinterpret_cast<unsigned char*>(&pss_params);
      param.len = sizeof(pss_params);
    } else if (SSL_get_signature_algorithm_key_type(algorithm) ==
               EVP_PKEY_RSA) {
      // PK11_SignWithMechanism expects the caller to prepend the DigestInfo for
      // PKCS #1.
      int hash_nid = EVP_MD_type(SSL_get_signature_algorithm_digest(algorithm));
      int is_alloced;
      size_t prefix_len;
      if (!RSA_add_pkcs1_prefix(&digest_item.data, &prefix_len, &is_alloced,
                                hash_nid, digest_item.data, digest_item.len)) {
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
      digest_item.len = prefix_len;
      if (is_alloced)
        free_digest_info.reset(digest_item.data);
    }

    {
      const int len = PK11_SignatureLen(key_.get());
      if (len <= 0) {
        LogPRError("PK11_SignatureLen failed");
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
      signature->resize(len);
      SECItem signature_item;
      signature_item.data = signature->data();
      signature_item.len = signature->size();

      SECStatus rv = PK11_SignWithMechanism(key_.get(), mechanism, &param,
                                            &signature_item, &digest_item);
      if (rv != SECSuccess) {
        LogPRError("PK11_SignWithMechanism failed");
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
      signature->resize(signature_item.len);
    }

    // NSS emits raw ECDSA signatures, but BoringSSL expects a DER-encoded
    // ECDSA-Sig-Value.
    if (SSL_get_signature_algorithm_key_type(algorithm) == EVP_PKEY_EC) {
      if (signature->size() % 2 != 0) {
        LOG(ERROR) << "Bad signature length";
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }
      size_t order_len = signature->size() / 2;

      // Convert the RAW ECDSA signature to a DER-encoded ECDSA-Sig-Value.
      bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_SIG_new());
      if (!sig || !BN_bin2bn(signature->data(), order_len, sig->r) ||
          !BN_bin2bn(signature->data() + order_len, order_len, sig->s)) {
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      }

      {
        const int len = i2d_ECDSA_SIG(sig.get(), nullptr);
        if (len <= 0)
          return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
        signature->resize(len);
      }

      {
        uint8_t* ptr = signature->data();
        const int len = i2d_ECDSA_SIG(sig.get(), &ptr);
        if (len <= 0)
          return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
        signature->resize(len);
      }
    }

    return OK;
  }

 private:
  int type_;
  // NSS retains a pointer to the password delegate, so retain a reference here
  // to ensure the lifetimes are correct.
  scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
      password_delegate_;
  crypto::ScopedSECKEYPrivateKey key_;
  bool supports_pss_;
};

}  // namespace

scoped_refptr<SSLPrivateKey> FetchClientCertPrivateKey(
    const X509Certificate* certificate,
    CERTCertificate* cert_certificate,
    scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
        password_delegate) {
  // This function may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  void* wincx = password_delegate ? password_delegate->wincx() : nullptr;
  crypto::ScopedSECKEYPrivateKey key(
      PK11_FindKeyByAnyCert(cert_certificate, wincx));
  if (!key)
    return nullptr;

  int type;
  size_t max_length;
  if (!GetClientCertInfo(certificate, &type, &max_length))
    return nullptr;

  // Note that key contains a reference to password_delegate->wincx() and may
  // use it in PK11_Sign. Thus password_delegate must outlive key. We pass it
  // into SSLPlatformKeyNSS to tie the lifetimes together. See
  // https://crbug.com/779090.
  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<SSLPlatformKeyNSS>(type, std::move(password_delegate),
                                          std::move(key)),
      GetSSLPlatformKeyTaskRunner());
}

}  // namespace net
