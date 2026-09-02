// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_NSS_H_
#define NET_CERT_INTERNAL_TRUST_STORE_NSS_H_

#include <cert.h>
#include <certt.h>

#include <variant>
#include <vector>

#include "crypto/scoped_nss_types.h"
#include "net/base/net_export.h"
#include "net/cert/internal/platform_trust_store.h"
#include "net/cert/scoped_nss_types.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

// TrustStoreNSS is an implementation of bssl::TrustStore which uses NSS to find
// trust anchors for path building. This bssl::TrustStore is thread-safe.
class NET_EXPORT TrustStoreNSS : public PlatformTrustStore {
 public:
  // Creates a TrustStoreNSS which will find anchors that are trusted for
  // SSL server auth. (Trust settings from the builtin roots slot with the
  // Mozilla CA Policy attribute will not be used.)
  TrustStoreNSS();

  TrustStoreNSS(const TrustStoreNSS&) = delete;
  TrustStoreNSS& operator=(const TrustStoreNSS&) = delete;

  ~TrustStoreNSS() override;

  // bssl::CertIssuerSource implementation:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;

  // bssl::TrustStore implementation:
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) override;
  std::shared_ptr<const bssl::MTCAnchor> GetTrustedMTCIssuerOf(
      const bssl::ParsedCertificate* cert) override;

  // net::PlatformTrustStore implementation:
  std::vector<net::PlatformTrustStore::CertWithTrust> GetAllUserAddedCerts()
      override;

 private:
  struct ListCertsResult {
    ListCertsResult(ScopedCERTCertificate cert, bssl::CertificateTrust trust);
    ~ListCertsResult();
    ListCertsResult(ListCertsResult&& other);
    ListCertsResult& operator=(ListCertsResult&& other);

    ScopedCERTCertificate cert;
    bssl::CertificateTrust trust;
  };
  std::vector<ListCertsResult> ListCertsIgnoringNSSRootsImpl(
      bool ignore_chaps_module);

  bssl::CertificateTrust GetTrustForNSSTrust(const CERTCertTrust& trust) const;

  bssl::CertificateTrust GetTrustIgnoringSystemTrust(
      CERTCertificate* nss_cert) const;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_NSS_H_
