// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_PKI_TRUST_STORE_H_
#define NET_CERT_PKI_TRUST_STORE_H_

#include <vector>

#include "base/supports_user_data.h"
#include "net/base/net_export.h"
#include "net/cert/pki/cert_issuer_source.h"
#include "net/cert/pki/parsed_certificate.h"

namespace net {

enum class CertificateTrustType {
  // This certificate is explicitly blocked (distrusted).
  DISTRUSTED,

  // The trustedness of this certificate is unknown (inherits trust from
  // its issuer).
  UNSPECIFIED,

  // This certificate is a trust anchor (as defined by RFC 5280). The only
  // fields in the certificate that are meaningful are its name and SPKI.
  TRUSTED_ANCHOR,

  // This certificate is a trust anchor which additionally has expiration
  // enforced. The only fields in the certificate that are meaningful are its
  // name, SPKI, and validity period.
  TRUSTED_ANCHOR_WITH_EXPIRATION,

  // This certificate is a trust anchor for which some of the fields in the
  // certificate (in addition to the name and SPKI) should be used during the
  // verification process. See VerifyCertificateChain() for details on how
  // constraints are applied.
  TRUSTED_ANCHOR_WITH_CONSTRAINTS,

  LAST = TRUSTED_ANCHOR_WITH_CONSTRAINTS
};

// Describes the level of trust in a certificate. See CertificateTrustType for
// details.
//
// TODO(eroman): Right now this is just a glorified wrapper around an enum...
struct NET_EXPORT CertificateTrust {
  static CertificateTrust ForTrustAnchor();
  static CertificateTrust ForTrustAnchorEnforcingExpiration();
  static CertificateTrust ForTrustAnchorEnforcingConstraints();
  static CertificateTrust ForUnspecified();
  static CertificateTrust ForDistrusted();

  bool IsTrustAnchor() const;
  bool IsDistrusted() const;
  bool HasUnspecifiedTrust() const;

  CertificateTrustType type = CertificateTrustType::UNSPECIFIED;
};

// Interface for finding intermediates / trust anchors, and testing the
// trustedness of certificates.
class NET_EXPORT TrustStore : public CertIssuerSource {
 public:
  TrustStore();

  TrustStore(const TrustStore&) = delete;
  TrustStore& operator=(const TrustStore&) = delete;

  // Returns the trusted of |cert|, which must be non-null.
  //
  // Optionally, if |debug_data| is non-null, debug information may be added
  // (any added Data must implement the Clone method.) The same |debug_data|
  // object may be passed to multiple GetTrust calls for a single verification,
  // so implementations should check whether they already added data with a
  // certain key and update it instead of overwriting it.
  virtual CertificateTrust GetTrust(
      const ParsedCertificate* cert,
      base::SupportsUserData* debug_data) const = 0;

  // Disable async issuers for TrustStore, as it isn't needed.
  // TODO(mattm): Pass debug_data here too.
  void AsyncGetIssuersOf(const ParsedCertificate* cert,
                         std::unique_ptr<Request>* out_req) final;
};

}  // namespace net

#endif  // NET_CERT_PKI_TRUST_STORE_H_
