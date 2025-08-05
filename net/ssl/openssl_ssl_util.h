// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_OPENSSL_SSL_UTIL_H_
#define NET_SSL_OPENSSL_SSL_UTIL_H_

#include <stdint.h>

#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_event_type.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace crypto {
class OpenSSLErrStackTracer;
}

namespace base {
class Location;
}

namespace net {

class NetLogWithSource;

// Puts a net error, |err|, on the error stack in OpenSSL. The file and line are
// extracted from |posted_from|. The function code of the error is left as 0.
void OpenSSLPutNetError(const base::Location& posted_from, int err);

// Converts an OpenSSL error code into a net error code, walking the OpenSSL
// error stack if needed.
//
// Note that |tracer| is not currently used in the implementation, but is passed
// in anyway as this ensures the caller will clear any residual codes left on
// the error stack.
NET_EXPORT_PRIVATE int MapOpenSSLError(
    int err,
    const crypto::OpenSSLErrStackTracer& tracer);

// Helper struct to store information about an OpenSSL error stack entry.
struct OpenSSLErrorInfo {
  OpenSSLErrorInfo() = default;

  uint32_t error_code = 0;
  const char* file = nullptr;
  int line = 0;
};

// Converts an OpenSSL error code into a net error code, walking the OpenSSL
// error stack if needed. If a value on the stack is used, the error code and
// associated information are returned in |*out_error_info|. Otherwise its
// fields are set to 0 and NULL. This function will never return OK, so
// SSL_ERROR_ZERO_RETURN must be handled externally.
//
// Note that |tracer| is not currently used in the implementation, but is passed
// in anyway as this ensures the caller will clear any residual codes left on
// the error stack.
int MapOpenSSLErrorWithDetails(int err,
                               const crypto::OpenSSLErrStackTracer& tracer,
                               OpenSSLErrorInfo* out_error_info);

// Logs an OpenSSL error to the NetLog.
void NetLogOpenSSLError(const NetLogWithSource& net_log,
                        NetLogEventType type,
                        int net_error,
                        int ssl_error,
                        const OpenSSLErrorInfo& error_info);

// Returns the net SSL version number (see ssl_connection_status_flags.h) for
// this SSL connection.
int GetNetSSLVersion(SSL* ssl);

// Returns a vector containing a pointer to the leaf certificate in `cert`
// followed by pointers to the intermediate certificates, suitable for passing
// via `ConfigureSSLCredentialParams`.
std::vector<CRYPTO_BUFFER*> GetCertChainRawVector(X509Certificate& cert);

// Converts `cert_chain` to a vector of raw pointers, suitable for passing via
// `ConfigureSSLCredentialParams`.
std::vector<CRYPTO_BUFFER*> GetCertChainRawVector(
    const std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>& cert_chain);

// Holds params for `ConfigureSSLCredential()`.
struct ConfigureSSLCredentialParams {
  STACK_ALLOCATED();  // Allow members to be spans instead of raw_spans.
 public:
  using PrivateKeyVariant =
      std::variant<EVP_PKEY*, const SSL_PRIVATE_KEY_METHOD*>;

  base::span<CRYPTO_BUFFER*> cert_chain;
  PrivateKeyVariant private_key;
  base::span<const uint16_t> signing_algorithm_prefs;
  base::span<const uint8_t> ocsp_response;
  base::span<const uint8_t> signed_cert_timestamp_list;
  base::span<const uint8_t> trust_anchor_id;
};

// Configures `ssl` to use the specified certificate and `params.private_key`
// as an available credential. This is a wrapper over |SSL_CREDENTIAL| APIs
// (https://commondatastorage.googleapis.com/chromium-boringssl-docs/ssl.h.html#Credentials).
//
// |params.signing_algorithm_prefs|, |params.ocsp_response|, and
// |params.signed_cert_timestamp| are configured with the respective
// SSL_CREDENTIAL APIs if non-empty.
//
// If |params.trust_anchor_id| is non-empty, it will be configured as the
// certificate's corresponding TLS Trust Anchor ID, and
// `SSL_CREDENTIAL_set_must_match_issuer` will be set to true
// (https://commondatastorage.googleapis.com/chromium-boringssl-docs/ssl.h.html#SSL_CREDENTIAL_set_must_match_issuer).
bool ConfigureSSLCredential(SSL* ssl, ConfigureSSLCredentialParams params);

}  // namespace net

#endif  // NET_SSL_OPENSSL_SSL_UTIL_H_
