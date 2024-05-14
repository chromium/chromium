// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_cipher_suite_names.h"

#include <ostream>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

int ObsoleteSSLStatusForProtocol(int ssl_version) {
  int obsolete_ssl = OBSOLETE_SSL_NONE;
  if (ssl_version < SSL_CONNECTION_VERSION_TLS1_2)
    obsolete_ssl |= OBSOLETE_SSL_MASK_PROTOCOL;
  return obsolete_ssl;
}

int ObsoleteSSLStatusForCipherSuite(uint16_t cipher_suite) {
  int obsolete_ssl = OBSOLETE_SSL_NONE;

  const SSL_CIPHER* cipher = SSL_get_cipher_by_value(cipher_suite);
  if (!cipher) {
    // Cannot determine/unknown cipher suite. Err on the side of caution.
    obsolete_ssl |= OBSOLETE_SSL_MASK_KEY_EXCHANGE;
    obsolete_ssl |= OBSOLETE_SSL_MASK_CIPHER;
    return obsolete_ssl;
  }

  if (SSL_CIPHER_get_kx_nid(cipher) == NID_kx_rsa) {
    obsolete_ssl |= OBSOLETE_SSL_MASK_KEY_EXCHANGE;
  }

  if (!SSL_CIPHER_is_aead(cipher)) {
    obsolete_ssl |= OBSOLETE_SSL_MASK_CIPHER;
  }

  return obsolete_ssl;
}

int ObsoleteSSLStatusForSignature(uint16_t signature_algorithm) {
  switch (signature_algorithm) {
    case SSL_SIGN_ECDSA_SHA1:
    case SSL_SIGN_RSA_PKCS1_MD5_SHA1:
    case SSL_SIGN_RSA_PKCS1_SHA1:
      return OBSOLETE_SSL_MASK_SIGNATURE;
    default:
      return OBSOLETE_SSL_NONE;
  }
}

}  // namespace

void SSLCipherSuiteToStrings(const char** key_exchange_str,
                             const char** cipher_str,
                             const char** mac_str,
                             bool* is_aead,
                             bool* is_tls13,
                             uint16_t cipher_suite) {
  *key_exchange_str = *cipher_str = *mac_str = "???";
  *is_aead = false;
  *is_tls13 = false;

  const SSL_CIPHER* cipher = SSL_get_cipher_by_value(cipher_suite);
  if (!cipher)
    return;

  switch (SSL_CIPHER_get_kx_nid(cipher)) {
    case NID_kx_any:
      *key_exchange_str = nullptr;
      *is_tls13 = true;
      break;
    case NID_kx_rsa:
      *key_exchange_str = "RSA";
      break;
    case NID_kx_ecdhe:
      switch (SSL_CIPHER_get_auth_nid(cipher)) {
        case NID_auth_rsa:
          *key_exchange_str = "ECDHE_RSA";
          break;
        case NID_auth_ecdsa:
          *key_exchange_str = "ECDHE_ECDSA";
          break;
      }
      break;
  }

  switch (SSL_CIPHER_get_cipher_nid(cipher)) {
    case NID_aes_128_gcm:
      *cipher_str = "AES_128_GCM";
      break;
    case NID_aes_256_gcm:
      *cipher_str = "AES_256_GCM";
      break;
    case NID_chacha20_poly1305:
      *cipher_str = "CHACHA20_POLY1305";
      break;
    case NID_aes_128_cbc:
      *cipher_str = "AES_128_CBC";
      break;
    case NID_aes_256_cbc:
      *cipher_str = "AES_256_CBC";
      break;
    case NID_des_ede3_cbc:
      *cipher_str = "3DES_EDE_CBC";
      break;
  }

  if (SSL_CIPHER_is_aead(cipher)) {
    *is_aead = true;
    *mac_str = nullptr;
  } else {
    switch (SSL_CIPHER_get_digest_nid(cipher)) {
      case NID_sha1:
        *mac_str = "HMAC-SHA1";
        break;
      case NID_sha256:
        *mac_str = "HMAC-SHA256";
        break;
      case NID_sha384:
        *mac_str = "HMAC-SHA384";
        break;
    }
  }
}

void SSLVersionToString(const char** name, int ssl_version) {
  switch (ssl_version) {
    case SSL_CONNECTION_VERSION_SSL2:
      *name = "SSL 2.0";
      break;
    case SSL_CONNECTION_VERSION_SSL3:
      *name = "SSL 3.0";
      break;
    case SSL_CONNECTION_VERSION_TLS1:
      *name = "TLS 1.0";
      break;
    case SSL_CONNECTION_VERSION_TLS1_1:
      *name = "TLS 1.1";
      break;
    case SSL_CONNECTION_VERSION_TLS1_2:
      *name = "TLS 1.2";
      break;
    case SSL_CONNECTION_VERSION_TLS1_3:
      *name = "TLS 1.3";
      break;
    case SSL_CONNECTION_VERSION_QUIC:
      *name = "QUIC";
      break;
    default:
      NOTREACHED_IN_MIGRATION() << ssl_version;
      *name = "???";
      break;
  }
}

bool ParseSSLCipherString(const std::string& cipher_string,
                          uint16_t* cipher_suite) {
  int value = 0;
  if (cipher_string.size() == 6 &&
      base::StartsWith(cipher_string, "0x",
                       base::CompareCase::INSENSITIVE_ASCII) &&
      base::HexStringToInt(cipher_string, &value)) {
    *cipher_suite = static_cast<uint16_t>(value);
    return true;
  }
  return false;
}

int ObsoleteSSLStatus(int connection_status, uint16_t signature_algorithm) {
  int obsolete_ssl = OBSOLETE_SSL_NONE;

  int ssl_version = SSLConnectionStatusToVersion(connection_status);
  obsolete_ssl |= ObsoleteSSLStatusForProtocol(ssl_version);

  uint16_t cipher_suite = SSLConnectionStatusToCipherSuite(connection_status);
  obsolete_ssl |= ObsoleteSSLStatusForCipherSuite(cipher_suite);

  obsolete_ssl |= ObsoleteSSLStatusForSignature(signature_algorithm);

  return obsolete_ssl;
}

bool IsTLSCipherSuiteAllowedByHTTP2(uint16_t cipher_suite) {
  return ObsoleteSSLStatusForCipherSuite(cipher_suite) == OBSOLETE_SSL_NONE;
}

}  // namespace net
