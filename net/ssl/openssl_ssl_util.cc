// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_ssl_util.h"

#include <errno.h>

#include <type_traits>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/openssl_util.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

class OpenSSLNetErrorLibSingleton {
 public:
  OpenSSLNetErrorLibSingleton() {
    // Allocate a new error library value for inserting net errors into
    // OpenSSL. This does not register any ERR_STRING_DATA for the errors, so
    // stringifying error codes through OpenSSL will return NULL.
    net_error_lib_ = ERR_get_next_error_library();
  }

  int net_error_lib() const { return net_error_lib_; }

 private:
  int net_error_lib_;
};

int OpenSSLNetErrorLib() {
  static_assert(
      std::is_trivially_destructible<OpenSSLNetErrorLibSingleton>::value);
  static OpenSSLNetErrorLibSingleton instance;
  return instance.net_error_lib();
}

int MapOpenSSLErrorSSL(uint32_t error_code) {
  DCHECK_EQ(ERR_LIB_SSL, ERR_GET_LIB(error_code));

#if DCHECK_IS_ON()
  char buf[ERR_ERROR_STRING_BUF_LEN];
  ERR_error_string_n(error_code, buf, sizeof(buf));
  DVLOG(1) << "OpenSSL SSL error, reason: " << ERR_GET_REASON(error_code)
           << ", name: " << buf;
#endif

  switch (ERR_GET_REASON(error_code)) {
    case SSL_R_READ_TIMEOUT_EXPIRED:
      return ERR_TIMED_OUT;
    case SSL_R_UNKNOWN_CERTIFICATE_TYPE:
    case SSL_R_UNKNOWN_CIPHER_TYPE:
    case SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE:
    case SSL_R_UNKNOWN_SSL_VERSION:
      return ERR_NOT_IMPLEMENTED;
    case SSL_R_NO_CIPHER_MATCH:
    case SSL_R_NO_SHARED_CIPHER:
    case SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY:
    case SSL_R_TLSV1_ALERT_PROTOCOL_VERSION:
    case SSL_R_UNSUPPORTED_PROTOCOL:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
    case SSL_R_TLSV1_ALERT_ACCESS_DENIED:
    case SSL_R_TLSV1_ALERT_CERTIFICATE_REQUIRED:
    case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    case SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_R_SSLV3_ALERT_BAD_RECORD_MAC:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_R_TLSV1_ALERT_DECRYPT_ERROR:
      return ERR_SSL_DECRYPT_ERROR_ALERT;
    case SSL_R_TLSV1_UNRECOGNIZED_NAME:
      return ERR_SSL_UNRECOGNIZED_NAME_ALERT;
    case SSL_R_SERVER_CERT_CHANGED:
      return ERR_SSL_SERVER_CERT_CHANGED;
    case SSL_R_WRONG_VERSION_ON_EARLY_DATA:
      return ERR_WRONG_VERSION_ON_EARLY_DATA;
    case SSL_R_TLS13_DOWNGRADE:
      return ERR_TLS13_DOWNGRADE_DETECTED;
    case SSL_R_ECH_REJECTED:
      return ERR_ECH_NOT_NEGOTIATED;
    // SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE may be returned from the server after
    // receiving ClientHello if there's no common supported cipher. Map that
    // specific case to ERR_SSL_VERSION_OR_CIPHER_MISMATCH to match the NSS
    // implementation. See https://goo.gl/oMtZW and https://crbug.com/446505.
    case SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE: {
      uint32_t previous = ERR_peek_error();
      if (previous != 0 && ERR_GET_LIB(previous) == ERR_LIB_SSL &&
          ERR_GET_REASON(previous) == SSL_R_HANDSHAKE_FAILURE_ON_CLIENT_HELLO) {
        return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
      }
      return ERR_SSL_PROTOCOL_ERROR;
    }
    case SSL_R_KEY_USAGE_BIT_INCORRECT:
      return ERR_SSL_KEY_USAGE_INCOMPATIBLE;
    default:
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

base::Value::Dict NetLogOpenSSLErrorParams(int net_error,
                                           int ssl_error,
                                           const OpenSSLErrorInfo& error_info) {
  base::Value::Dict dict;
  dict.Set("net_error", net_error);
  dict.Set("ssl_error", ssl_error);
  if (error_info.error_code != 0) {
    dict.Set("error_lib", ERR_GET_LIB(error_info.error_code));
    dict.Set("error_reason", ERR_GET_REASON(error_info.error_code));
  }
  if (error_info.file != nullptr)
    dict.Set("file", error_info.file);
  if (error_info.line != 0)
    dict.Set("line", error_info.line);
  return dict;
}

}  // namespace

void OpenSSLPutNetError(const base::Location& location, int err) {
  // Net error codes are negative. Encode them as positive numbers.
  err = -err;
  if (err < 0 || err > 0xfff) {
    // OpenSSL reserves 12 bits for the reason code.
    NOTREACHED();
  }
  ERR_put_error(OpenSSLNetErrorLib(), 0 /* unused */, err, location.file_name(),
                location.line_number());
}

int MapOpenSSLError(int err, const crypto::OpenSSLErrStackTracer& tracer) {
  OpenSSLErrorInfo error_info;
  return MapOpenSSLErrorWithDetails(err, tracer, &error_info);
}

int MapOpenSSLErrorWithDetails(int err,
                               const crypto::OpenSSLErrStackTracer& tracer,
                               OpenSSLErrorInfo* out_error_info) {
  *out_error_info = OpenSSLErrorInfo();

  switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return ERR_IO_PENDING;
    case SSL_ERROR_EARLY_DATA_REJECTED:
      return ERR_EARLY_DATA_REJECTED;
    case SSL_ERROR_SYSCALL:
      PLOG(ERROR) << "OpenSSL SYSCALL error, earliest error code in "
                     "error queue: "
                  << ERR_peek_error();
      return ERR_FAILED;
    case SSL_ERROR_SSL:
      // Walk down the error stack to find an SSL or net error.
      while (true) {
        OpenSSLErrorInfo error_info;
        error_info.error_code =
            ERR_get_error_line(&error_info.file, &error_info.line);
        if (error_info.error_code == 0) {
          // Map errors to ERR_SSL_PROTOCOL_ERROR by default, reporting the most
          // recent error in |*out_error_info|.
          return ERR_SSL_PROTOCOL_ERROR;
        }

        *out_error_info = error_info;
        if (ERR_GET_LIB(error_info.error_code) == ERR_LIB_SSL) {
          return MapOpenSSLErrorSSL(error_info.error_code);
        }
        if (ERR_GET_LIB(error_info.error_code) == OpenSSLNetErrorLib()) {
          // Net error codes are negative but encoded in OpenSSL as positive
          // numbers.
          return -ERR_GET_REASON(error_info.error_code);
        }
      }
    default:
      // TODO(joth): Implement full mapping.
      LOG(WARNING) << "Unknown OpenSSL error " << err;
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

void NetLogOpenSSLError(const NetLogWithSource& net_log,
                        NetLogEventType type,
                        int net_error,
                        int ssl_error,
                        const OpenSSLErrorInfo& error_info) {
  net_log.AddEvent(type, [&] {
    return NetLogOpenSSLErrorParams(net_error, ssl_error, error_info);
  });
}

int GetNetSSLVersion(SSL* ssl) {
  switch (SSL_version(ssl)) {
    case TLS1_VERSION:
      return SSL_CONNECTION_VERSION_TLS1;
    case TLS1_1_VERSION:
      return SSL_CONNECTION_VERSION_TLS1_1;
    case TLS1_2_VERSION:
      return SSL_CONNECTION_VERSION_TLS1_2;
    case TLS1_3_VERSION:
      return SSL_CONNECTION_VERSION_TLS1_3;
    default:
      NOTREACHED();
  }
}

std::vector<CRYPTO_BUFFER*> GetCertChainRawVector(X509Certificate& cert) {
  std::vector<CRYPTO_BUFFER*> chain_raw;
  chain_raw.reserve(cert.cert_buffers().size());
  for (const auto& handle : cert.cert_buffers()) {
    chain_raw.push_back(handle.get());
  }
  return chain_raw;
}

std::vector<CRYPTO_BUFFER*> GetCertChainRawVector(
    const std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>& cert_chain) {
  std::vector<CRYPTO_BUFFER*> chain_raw;
  chain_raw.reserve(cert_chain.size());
  for (const auto& handle : cert_chain) {
    chain_raw.push_back(handle.get());
  }
  return chain_raw;
}

bool ConfigureSSLCredential(SSL* ssl, ConfigureSSLCredentialParams params) {
  bssl::UniquePtr<SSL_CREDENTIAL> credential(SSL_CREDENTIAL_new_x509());
  if (!credential) {
    return false;
  }

  if (!SSL_CREDENTIAL_set1_cert_chain(credential.get(),
                                      params.cert_chain.data(),
                                      params.cert_chain.size())) {
    return false;
  }
  if (!params.signing_algorithm_prefs.empty()) {
    if (!SSL_CREDENTIAL_set1_signing_algorithm_prefs(
            credential.get(), params.signing_algorithm_prefs.data(),
            params.signing_algorithm_prefs.size())) {
      return false;
    }
  }

  if (std::holds_alternative<EVP_PKEY*>(params.private_key)) {
    EVP_PKEY* pkey = std::get<EVP_PKEY*>(params.private_key);
    CHECK(pkey);
    if (!SSL_CREDENTIAL_set1_private_key(credential.get(), pkey)) {
      return false;
    }
  } else {
    const SSL_PRIVATE_KEY_METHOD* custom_key =
        std::get<const SSL_PRIVATE_KEY_METHOD*>(params.private_key);
    CHECK(custom_key);
    if (!SSL_CREDENTIAL_set_private_key_method(credential.get(), custom_key)) {
      return false;
    }
  }

  if (!params.ocsp_response.empty()) {
    bssl::UniquePtr<CRYPTO_BUFFER> buf(CRYPTO_BUFFER_new(
        params.ocsp_response.data(), params.ocsp_response.size(), nullptr));
    if (!SSL_CREDENTIAL_set1_ocsp_response(credential.get(), buf.get())) {
      return false;
    }
  }

  if (!params.signed_cert_timestamp_list.empty()) {
    bssl::UniquePtr<CRYPTO_BUFFER> buf(
        CRYPTO_BUFFER_new(params.signed_cert_timestamp_list.data(),
                          params.signed_cert_timestamp_list.size(), nullptr));
    if (!SSL_CREDENTIAL_set1_signed_cert_timestamp_list(credential.get(),
                                                        buf.get())) {
      return false;
    }
  }

  if (!params.trust_anchor_id.empty()) {
    if (!SSL_CREDENTIAL_set1_trust_anchor_id(credential.get(),
                                             params.trust_anchor_id.data(),
                                             params.trust_anchor_id.size())) {
      return false;
    }
    SSL_CREDENTIAL_set_must_match_issuer(credential.get(), 1);
  }

  if (!SSL_add1_credential(ssl, credential.get())) {
    LOG(WARNING) << "Failed to set certificate";
    return false;
  }

  return true;
}

}  // namespace net
