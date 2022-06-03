// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_private_key.h"

#include "base/notreached.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

std::vector<uint16_t> SSLPrivateKey::DefaultAlgorithmPreferences(
    int type,
    bool supports_pss) {
  switch (type) {
    case EVP_PKEY_RSA:
      if (supports_pss) {
        return {
            // Only SHA-1 if the server supports no other hashes, but otherwise
            // prefer smaller SHA-2 hashes. SHA-256 is considered fine and more
            // likely to be supported by smartcards, etc.
            SSL_SIGN_RSA_PKCS1_SHA256, SSL_SIGN_RSA_PKCS1_SHA384,
            SSL_SIGN_RSA_PKCS1_SHA512, SSL_SIGN_RSA_PKCS1_SHA1,

            // Order PSS last so we preferentially use the more conservative
            // option. While the platform APIs may support RSA-PSS, the key may
            // not. Ideally the SSLPrivateKey would query this, but smartcards
            // often do not support such queries well.
            SSL_SIGN_RSA_PSS_SHA256, SSL_SIGN_RSA_PSS_SHA384,
            SSL_SIGN_RSA_PSS_SHA512,
        };
      }
      return {
          SSL_SIGN_RSA_PKCS1_SHA256, SSL_SIGN_RSA_PKCS1_SHA384,
          SSL_SIGN_RSA_PKCS1_SHA512, SSL_SIGN_RSA_PKCS1_SHA1,
      };
    case EVP_PKEY_EC:
      return {
          SSL_SIGN_ECDSA_SECP256R1_SHA256, SSL_SIGN_ECDSA_SECP384R1_SHA384,
          SSL_SIGN_ECDSA_SECP521R1_SHA512, SSL_SIGN_ECDSA_SHA1,
      };
    default:
      NOTIMPLEMENTED();
      return {};
  };
}

}  // namespace net
