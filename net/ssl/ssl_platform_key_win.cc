// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_win.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

std::string GetCAPIProviderName(HCRYPTPROV provider) {
  DWORD name_len;
  if (!CryptGetProvParam(provider, PP_NAME, nullptr, &name_len, 0)) {
    return "(error getting name)";
  }
  std::vector<BYTE> name(name_len);
  if (!CryptGetProvParam(provider, PP_NAME, name.data(), &name_len, 0)) {
    return "(error getting name)";
  }
  // Per Microsoft's documentation, PP_NAME is NUL-terminated. However,
  // smartcard drivers are notoriously buggy, so check this.
  auto nul = std::find(name.begin(), name.end(), 0);
  if (nul != name.end()) {
    name_len = nul - name.begin();
  }
  return std::string(reinterpret_cast<const char*>(name.data()), name_len);
}

class SSLPlatformKeyCAPI : public ThreadedSSLPrivateKey::Delegate {
 public:
  // Takes ownership of |provider|.
  SSLPlatformKeyCAPI(HCRYPTPROV provider, DWORD key_spec)
      : provider_name_(GetCAPIProviderName(provider)),
        provider_(provider),
        key_spec_(key_spec) {}

  ~SSLPlatformKeyCAPI() override {}

  std::string GetProviderName() override { return "CAPI: " + provider_name_; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    // If the key is in CAPI, assume conservatively that the CAPI service
    // provider may only be able to sign pre-TLS-1.2 and SHA-1 hashes.
    // Prioritize SHA-1, but if the server doesn't advertise it, leave the other
    // algorithms enabled to try.
    return {
        SSL_SIGN_RSA_PKCS1_SHA1, SSL_SIGN_RSA_PKCS1_SHA256,
        SSL_SIGN_RSA_PKCS1_SHA384, SSL_SIGN_RSA_PKCS1_SHA512,
    };
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

    ALG_ID hash_alg;
    switch (EVP_MD_type(md)) {
      case NID_md5_sha1:
        hash_alg = CALG_SSL3_SHAMD5;
        break;
      case NID_sha1:
        hash_alg = CALG_SHA1;
        break;
      case NID_sha256:
        hash_alg = CALG_SHA_256;
        break;
      case NID_sha384:
        hash_alg = CALG_SHA_384;
        break;
      case NID_sha512:
        hash_alg = CALG_SHA_512;
        break;
      default:
        NOTREACHED();
        return ERR_FAILED;
    }

    crypto::ScopedHCRYPTHASH hash_handle;
    if (!CryptCreateHash(provider_, hash_alg, 0, 0, hash_handle.receive())) {
      PLOG(ERROR) << "CreateCreateHash failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    DWORD hash_len;
    DWORD arg_len = sizeof(hash_len);
    if (!CryptGetHashParam(hash_handle.get(), HP_HASHSIZE,
                           reinterpret_cast<BYTE*>(&hash_len), &arg_len, 0)) {
      PLOG(ERROR) << "CryptGetHashParam HP_HASHSIZE failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    if (hash_len != digest_len)
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    if (!CryptSetHashParam(hash_handle.get(), HP_HASHVAL,
                           const_cast<BYTE*>(digest), 0)) {
      PLOG(ERROR) << "CryptSetHashParam HP_HASHVAL failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    DWORD signature_len = 0;
    if (!CryptSignHash(hash_handle.get(), key_spec_, nullptr, 0, nullptr,
                       &signature_len)) {
      PLOG(ERROR) << "CryptSignHash failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(signature_len);
    if (!CryptSignHash(hash_handle.get(), key_spec_, nullptr, 0,
                       signature->data(), &signature_len)) {
      PLOG(ERROR) << "CryptSignHash failed";
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(signature_len);

    // CryptoAPI signs in little-endian, so reverse it.
    std::reverse(signature->begin(), signature->end());
    return OK;
  }

 private:
  std::string provider_name_;
  crypto::ScopedHCRYPTPROV provider_;
  DWORD key_spec_;

  DISALLOW_COPY_AND_ASSIGN(SSLPlatformKeyCAPI);
};

class ScopedNCRYPT_PROV_HANDLE {
 public:
  ScopedNCRYPT_PROV_HANDLE() {}
  ScopedNCRYPT_PROV_HANDLE(const ScopedNCRYPT_PROV_HANDLE&) = delete;
  ScopedNCRYPT_PROV_HANDLE& operator=(const ScopedNCRYPT_PROV_HANDLE&) = delete;
  ~ScopedNCRYPT_PROV_HANDLE() {
    if (prov_) {
      NCryptFreeObject(prov_);
    }
  }

  NCRYPT_PROV_HANDLE get() const { return prov_; }
  NCRYPT_PROV_HANDLE* InitializeInto() { return &prov_; }

 private:
  NCRYPT_PROV_HANDLE prov_ = 0;
};

std::string GetCNGProviderName(NCRYPT_KEY_HANDLE key) {
  ScopedNCRYPT_PROV_HANDLE prov;
  DWORD prov_len = 0;
  SECURITY_STATUS status = NCryptGetProperty(
      key, NCRYPT_PROVIDER_HANDLE_PROPERTY,
      reinterpret_cast<BYTE*>(prov.InitializeInto()),
      sizeof(*prov.InitializeInto()), &prov_len, NCRYPT_SILENT_FLAG);
  if (FAILED(status)) {
    return "(error getting provider)";
  }
  DCHECK_EQ(sizeof(NCRYPT_PROV_HANDLE), prov_len);

  // NCRYPT_NAME_PROPERTY is a NUL-terminated Unicode string, which means an
  // array of wchar_t, however NCryptGetProperty works in bytes, so lengths must
  // be converted.
  DWORD name_len = 0;
  status = NCryptGetProperty(prov.get(), NCRYPT_NAME_PROPERTY, nullptr, 0,
                             &name_len, NCRYPT_SILENT_FLAG);
  if (FAILED(status) || name_len % sizeof(wchar_t) != 0) {
    return "(error getting provider name)";
  }
  std::vector<wchar_t> name(name_len / sizeof(wchar_t));
  status = NCryptGetProperty(
      prov.get(), NCRYPT_NAME_PROPERTY, reinterpret_cast<BYTE*>(name.data()),
      name.size() * sizeof(wchar_t), &name_len, NCRYPT_SILENT_FLAG);
  if (FAILED(status)) {
    return "(error getting provider name)";
  }
  name.resize(name_len / sizeof(wchar_t));

  // Per Microsoft's documentation, the name is NUL-terminated. However,
  // smartcard drivers are notoriously buggy, so check this.
  auto nul = std::find(name.begin(), name.end(), 0);
  if (nul != name.end()) {
    name.erase(nul, name.end());
  }
  return base::WideToUTF8(base::WStringPiece(name.data(), name.size()));
}

class SSLPlatformKeyCNG : public ThreadedSSLPrivateKey::Delegate {
 public:
  // Takes ownership of |key|.
  SSLPlatformKeyCNG(NCRYPT_KEY_HANDLE key, int type, size_t max_length)
      : provider_name_(GetCNGProviderName(key)),
        key_(key),
        type_(type),
        max_length_(max_length) {}

  ~SSLPlatformKeyCNG() override { NCryptFreeObject(key_); }

  std::string GetProviderName() override { return "CNG: " + provider_name_; }

  std::vector<uint16_t> GetAlgorithmPreferences() override {
    // If this is an under 1024-bit RSA key, conservatively prefer to sign SHA-1
    // hashes. Older Estonian ID cards can only sign SHA-1 hashes.  Prioritize
    // SHA-1, but if the server doesn't advertise it, leave the other algorithms
    // enabled to try.
    if (type_ == EVP_PKEY_RSA && max_length_ <= 1024 / 8) {
      return {
          SSL_SIGN_RSA_PKCS1_SHA1, SSL_SIGN_RSA_PKCS1_SHA256,
          SSL_SIGN_RSA_PKCS1_SHA384, SSL_SIGN_RSA_PKCS1_SHA512,
          // 1024-bit keys are too small for SSL_SIGN_RSA_PSS_SHA512.
          SSL_SIGN_RSA_PSS_SHA256, SSL_SIGN_RSA_PSS_SHA384,
      };
    }
    return SSLPrivateKey::DefaultAlgorithmPreferences(type_,
                                                      true /* supports PSS */);
  }

  Error Sign(uint16_t algorithm,
             base::span<const uint8_t> input,
             std::vector<uint8_t>* signature) override {
    crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

    const EVP_MD* md = SSL_get_signature_algorithm_digest(algorithm);
    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned digest_len;
    if (!md || !EVP_Digest(input.data(), input.size(), digest, &digest_len, md,
                           nullptr)) {
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }

    BCRYPT_PKCS1_PADDING_INFO pkcs1_padding_info = {nullptr};
    BCRYPT_PSS_PADDING_INFO pss_padding_info = {nullptr};
    void* padding_info = nullptr;
    DWORD flags = 0;
    if (SSL_get_signature_algorithm_key_type(algorithm) == EVP_PKEY_RSA) {
      const WCHAR* hash_alg;
      switch (EVP_MD_type(md)) {
        case NID_md5_sha1:
          hash_alg = nullptr;
          break;
        case NID_sha1:
          hash_alg = BCRYPT_SHA1_ALGORITHM;
          break;
        case NID_sha256:
          hash_alg = BCRYPT_SHA256_ALGORITHM;
          break;
        case NID_sha384:
          hash_alg = BCRYPT_SHA384_ALGORITHM;
          break;
        case NID_sha512:
          hash_alg = BCRYPT_SHA512_ALGORITHM;
          break;
        default:
          NOTREACHED();
          return ERR_FAILED;
      }
      if (SSL_is_signature_algorithm_rsa_pss(algorithm)) {
        pss_padding_info.pszAlgId = hash_alg;
        pss_padding_info.cbSalt = EVP_MD_size(md);
        padding_info = &pss_padding_info;
        flags |= BCRYPT_PAD_PSS;
      } else {
        pkcs1_padding_info.pszAlgId = hash_alg;
        padding_info = &pkcs1_padding_info;
        flags |= BCRYPT_PAD_PKCS1;
      }
    }

    DWORD signature_len;
    SECURITY_STATUS status =
        NCryptSignHash(key_, padding_info, const_cast<BYTE*>(digest),
                       digest_len, nullptr, 0, &signature_len, flags);
    if (FAILED(status)) {
      LOG(ERROR) << "NCryptSignHash failed: " << status;
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(signature_len);
    status = NCryptSignHash(key_, padding_info, const_cast<BYTE*>(digest),
                            digest_len, signature->data(), signature_len,
                            &signature_len, flags);
    if (FAILED(status)) {
      LOG(ERROR) << "NCryptSignHash failed: " << status;
      return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
    }
    signature->resize(signature_len);

    // CNG emits raw ECDSA signatures, but BoringSSL expects a DER-encoded
    // ECDSA-Sig-Value.
    if (type_ == EVP_PKEY_EC) {
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

      int len = i2d_ECDSA_SIG(sig.get(), nullptr);
      if (len <= 0)
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      signature->resize(len);
      uint8_t* ptr = signature->data();
      len = i2d_ECDSA_SIG(sig.get(), &ptr);
      if (len <= 0)
        return ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED;
      signature->resize(len);
    }

    return OK;
  }

 private:
  std::string provider_name_;
  NCRYPT_KEY_HANDLE key_;
  int type_;
  size_t max_length_;

  DISALLOW_COPY_AND_ASSIGN(SSLPlatformKeyCNG);
};

}  // namespace

scoped_refptr<SSLPrivateKey> WrapCAPIPrivateKey(
    const X509Certificate* certificate,
    HCRYPTPROV prov,
    DWORD key_spec) {
  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<SSLPlatformKeyCAPI>(prov, key_spec),
      GetSSLPlatformKeyTaskRunner());
}

scoped_refptr<SSLPrivateKey> WrapCNGPrivateKey(
    const X509Certificate* certificate,
    NCRYPT_KEY_HANDLE key) {
  // Rather than query the private key for metadata, extract the public key from
  // the certificate without using Windows APIs. CNG does not consistently work
  // depending on the system. See https://crbug.com/468345.
  int key_type;
  size_t max_length;
  if (!GetClientCertInfo(certificate, &key_type, &max_length)) {
    NCryptFreeObject(key);
    return nullptr;
  }

  return base::MakeRefCounted<ThreadedSSLPrivateKey>(
      std::make_unique<SSLPlatformKeyCNG>(key, key_type, max_length),
      GetSSLPlatformKeyTaskRunner());
}

scoped_refptr<SSLPrivateKey> FetchClientCertPrivateKey(
    const X509Certificate* certificate,
    PCCERT_CONTEXT cert_context) {
  HCRYPTPROV_OR_NCRYPT_KEY_HANDLE prov_or_key = 0;
  DWORD key_spec = 0;
  BOOL must_free = FALSE;
  DWORD flags = CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG;

  if (!CryptAcquireCertificatePrivateKey(cert_context, flags, nullptr,
                                         &prov_or_key, &key_spec, &must_free)) {
    PLOG(WARNING) << "Could not acquire private key";
    return nullptr;
  }

  // Should never get a cached handle back - ownership must always be
  // transferred.
  CHECK_EQ(must_free, TRUE);

  if (key_spec == CERT_NCRYPT_KEY_SPEC) {
    return WrapCNGPrivateKey(certificate, prov_or_key);
  } else {
    return WrapCAPIPrivateKey(certificate, prov_or_key, key_spec);
  }
}

}  // namespace net
