// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/certificate_helpers.h"

#include <string>

#include "base/logging.h"
#include "net/cert/x509_certificate.h"

namespace remoting {

namespace {

constexpr char kCertIssuerWildCard[] = "*";

// Returns true if certificate |c1| is a worse match than |c2|.
//
// Criteria:
// 1. An invalid certificate is always worse than a valid certificate.
// 2. Invalid certificates are equally bad, in which case false will be
//    returned.
// 3. A certificate with earlier |valid_start| time is worse.
// 4. When |valid_start| are the same, the certificate with earlier
//    |valid_expiry| is worse.
bool WorseThan(const std::string& issuer,
               const base::Time& now,
               const net::X509Certificate& c1,
               const net::X509Certificate& c2) {
  if (!IsCertificateValid(issuer, now, c2)) {
    return false;
  }

  if (!IsCertificateValid(issuer, now, c1)) {
    return true;
  }

  if (c1.valid_start() != c2.valid_start()) {
    return c1.valid_start() < c2.valid_start();
  }

  return c1.valid_expiry() < c2.valid_expiry();
}

}  // namespace

std::string GetPreferredIssuerFieldValue(const net::X509Certificate& cert) {
  if (!cert.issuer().common_name.empty()) {
    return cert.issuer().common_name;
  }
  if (!cert.issuer().organization_names.empty() &&
      !cert.issuer().organization_names[0].empty()) {
    return cert.issuer().organization_names[0];
  }
  if (!cert.issuer().organization_unit_names.empty() &&
      !cert.issuer().organization_unit_names[0].empty()) {
    return cert.issuer().organization_unit_names[0];
  }

  return std::string();
}

bool IsCertificateValid(const std::string& issuer,
                        const base::Time& now,
                        const net::X509Certificate& cert) {
  return (issuer == kCertIssuerWildCard ||
          issuer == GetPreferredIssuerFieldValue(cert)) &&
         cert.valid_start() <= now && cert.valid_expiry() > now;
}

std::unique_ptr<net::ClientCertIdentity> GetBestMatchFromCertificateList(
    const std::string& issuer,
    const base::Time& now,
    net::ClientCertIdentityList& client_certs) {
  auto best_match_position = std::max_element(
      client_certs.begin(), client_certs.end(),
      [&issuer, now](std::unique_ptr<net::ClientCertIdentity>& i1,
                     std::unique_ptr<net::ClientCertIdentity>& i2) {
        return WorseThan(issuer, now, *i1->certificate(), *i2->certificate());
      });

  if (best_match_position == client_certs.end()) {
    LOG(ERROR) << "Failed to find a certificate from the list of candidates ("
               << client_certs.size() << ").";
    return nullptr;
  }

  return std::move(*best_match_position);
}

}  // namespace remoting
