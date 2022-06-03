// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store.h"

#include "base/notreached.h"

namespace net {

CertificateTrust CertificateTrust::ForTrustAnchor() {
  CertificateTrust result;
  result.type = CertificateTrustType::TRUSTED_ANCHOR;
  return result;
}

CertificateTrust CertificateTrust::ForTrustAnchorEnforcingConstraints() {
  CertificateTrust result;
  result.type = CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS;
  return result;
}

CertificateTrust CertificateTrust::ForUnspecified() {
  CertificateTrust result;
  result.type = CertificateTrustType::UNSPECIFIED;
  return result;
}

CertificateTrust CertificateTrust::ForDistrusted() {
  CertificateTrust result;
  result.type = CertificateTrustType::DISTRUSTED;
  return result;
}

bool CertificateTrust::IsTrustAnchor() const {
  switch (type) {
    case CertificateTrustType::DISTRUSTED:
    case CertificateTrustType::UNSPECIFIED:
      return false;
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS:
      return true;
  }

  NOTREACHED();
  return false;
}

bool CertificateTrust::IsDistrusted() const {
  switch (type) {
    case CertificateTrustType::DISTRUSTED:
      return true;
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS:
      return false;
  }

  NOTREACHED();
  return false;
}

bool CertificateTrust::HasUnspecifiedTrust() const {
  switch (type) {
    case CertificateTrustType::UNSPECIFIED:
      return true;
    case CertificateTrustType::DISTRUSTED:
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS:
      return false;
  }

  NOTREACHED();
  return true;
}

TrustStore::TrustStore() = default;

void TrustStore::AsyncGetIssuersOf(const ParsedCertificate* cert,
                                   std::unique_ptr<Request>* out_req) {
  out_req->reset();
}

}  // namespace net
