// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config.h"

#include "net/cert/cert_verifier.h"

namespace net {

// Note these lines must be kept in sync with
// services/network/public/mojom/ssl_config.mojom.
const uint16_t kDefaultSSLVersionMin = SSL_PROTOCOL_VERSION_TLS1;
const uint16_t kDefaultSSLVersionMinWarn = SSL_PROTOCOL_VERSION_TLS1_2;
const uint16_t kDefaultSSLVersionMax = SSL_PROTOCOL_VERSION_TLS1_3;

SSLConfig::CertAndStatus::CertAndStatus() = default;
SSLConfig::CertAndStatus::CertAndStatus(scoped_refptr<X509Certificate> cert_arg,
                                        CertStatus status)
    : cert(std::move(cert_arg)), cert_status(status) {}
SSLConfig::CertAndStatus::CertAndStatus(const CertAndStatus& other) = default;
SSLConfig::CertAndStatus::~CertAndStatus() = default;

SSLConfig::SSLConfig() = default;

SSLConfig::SSLConfig(const SSLConfig& other) = default;

SSLConfig::~SSLConfig() = default;

bool SSLConfig::IsAllowedBadCert(X509Certificate* cert,
                                 CertStatus* cert_status) const {
  for (const auto& allowed_bad_cert : allowed_bad_certs) {
    if (cert->EqualsExcludingChain(allowed_bad_cert.cert.get())) {
      if (cert_status)
        *cert_status = allowed_bad_cert.cert_status;
      return true;
    }
  }
  return false;
}

int SSLConfig::GetCertVerifyFlags() const {
  int flags = 0;
  if (disable_cert_verification_network_fetches)
    flags |= CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES;

  return flags;
}

}  // namespace net
