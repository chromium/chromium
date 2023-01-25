// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_PKI_TRUST_STORE_H_
#define NET_CERT_PKI_TRUST_STORE_H_

#include "base/supports_user_data.h"
#include "net/base/net_export.h"
#include "net/cert/pki/cert_issuer_source.h"
#include "net/cert/pki/parsed_certificate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

enum class CertificateTrustType {
  // This certificate is explicitly blocked (distrusted).
  DISTRUSTED,

  // The trustedness of this certificate is unknown (inherits trust from
  // its issuer).
  UNSPECIFIED,

  // This certificate is a trust anchor (as defined by RFC 5280).
  TRUSTED_ANCHOR,

  // This certificate can be used as a trust anchor (as defined by RFC 5280) or
  // a trusted leaf, depending on context.
  TRUSTED_ANCHOR_OR_LEAF,

  // This certificate is a directly trusted leaf.
  TRUSTED_LEAF,

  LAST = TRUSTED_ANCHOR
};

// Describes the level of trust in a certificate.
struct NET_EXPORT CertificateTrust {
  static constexpr CertificateTrust ForTrustAnchor() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_ANCHOR;
    return result;
  }

  static constexpr CertificateTrust ForTrustAnchorOrLeaf() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF;
    return result;
  }

  static constexpr CertificateTrust ForTrustedLeaf() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_LEAF;
    return result;
  }

  static constexpr CertificateTrust ForUnspecified() {
    CertificateTrust result;
    return result;
  }

  static constexpr CertificateTrust ForDistrusted() {
    CertificateTrust result;
    result.type = CertificateTrustType::DISTRUSTED;
    return result;
  }

  constexpr CertificateTrust WithEnforceAnchorExpiry(bool value = true) const {
    CertificateTrust result = *this;
    result.enforce_anchor_expiry = value;
    return result;
  }

  constexpr CertificateTrust WithEnforceAnchorConstraints(
      bool value = true) const {
    CertificateTrust result = *this;
    result.enforce_anchor_constraints = value;
    return result;
  }

  constexpr CertificateTrust WithRequireAnchorBasicConstraints(
      bool value = true) const {
    CertificateTrust result = *this;
    result.require_anchor_basic_constraints = value;
    return result;
  }

  constexpr CertificateTrust WithRequireLeafSelfSigned(
      bool value = true) const {
    CertificateTrust result = *this;
    result.require_leaf_selfsigned = value;
    return result;
  }

  bool IsTrustAnchor() const;
  bool IsTrustLeaf() const;
  bool IsDistrusted() const;
  bool HasUnspecifiedTrust() const;

  std::string ToDebugString() const;

  static absl::optional<CertificateTrust> FromDebugString(
      const std::string& trust_string);

  // The overall type of trust.
  CertificateTrustType type = CertificateTrustType::UNSPECIFIED;

  // Optionally, enforce extra bits on trust anchors. If these are false, the
  // only fields in a trust anchor certificate that are meaningful are its
  // name and SPKI.
  bool enforce_anchor_expiry = false;
  bool enforce_anchor_constraints = false;
  // Require that trust anchors have a basicConstraints extension. (This only
  // has effect if `enforce_anchor_constraints` is also true.)
  bool require_anchor_basic_constraints = false;

  // Optionally, require trusted leafs to be self-signed to be trusted.
  bool require_leaf_selfsigned = false;
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
  virtual CertificateTrust GetTrust(const ParsedCertificate* cert,
                                    base::SupportsUserData* debug_data) = 0;

  // Disable async issuers for TrustStore, as it isn't needed.
  // TODO(mattm): Pass debug_data here too.
  void AsyncGetIssuersOf(const ParsedCertificate* cert,
                         std::unique_ptr<Request>* out_req) final;
};

}  // namespace net

#endif  // NET_CERT_PKI_TRUST_STORE_H_
