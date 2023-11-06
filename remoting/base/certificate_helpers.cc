// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/certificate_helpers.h"

#include <string>

#include "base/logging.h"
#include "build/build_config.h"
#include "crypto/crypto_buildflags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_store.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#elif BUILDFLAG(IS_WIN)
#include "net/ssl/client_cert_store_win.h"
#elif BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_IOS)
#include "net/ssl/client_cert_store_mac.h"
#endif

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

#if BUILDFLAG(IS_WIN)
crypto::ScopedHCERTSTORE OpenLocalMachineCertStore() {
  return crypto::ScopedHCERTSTORE(::CertOpenStore(
      CERT_STORE_PROV_SYSTEM, 0, NULL,
      CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG, L"MY"));
}
#endif

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

std::unique_ptr<net::ClientCertStore> CreateClientCertStoreInstance() {
#if BUILDFLAG(USE_NSS_CERTS)
  return std::make_unique<net::ClientCertStoreNSS>(
      net::ClientCertStoreNSS::PasswordDelegateFactory());
#elif BUILDFLAG(IS_WIN)
  // The network process is running as "Local Service" whose "Current User"
  // cert store doesn't contain any certificates. Use the "Local Machine"
  // store instead.
  // The ACL on the private key of the machine certificate in the "Local
  // Machine" cert store needs to allow access by "Local Service".
  return std::make_unique<net::ClientCertStoreWin>(
      base::BindRepeating(&OpenLocalMachineCertStore));
#elif BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_IOS)
  return std::make_unique<net::ClientCertStoreMac>();
#else
  // OpenSSL does not use the ClientCertStore infrastructure.
  return nullptr;
#endif
}

}  // namespace remoting
