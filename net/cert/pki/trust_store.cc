// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/trust_store.h"

namespace net {

bool CertificateTrust::IsTrustAnchor() const {
  switch (type) {
    case CertificateTrustType::DISTRUSTED:
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_LEAF:
      return false;
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      return true;
  }

  assert(0);  // NOTREACHED
  return false;
}

bool CertificateTrust::IsTrustLeaf() const {
  switch (type) {
    case CertificateTrustType::TRUSTED_LEAF:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      return true;
    case CertificateTrustType::DISTRUSTED:
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_ANCHOR:
      return false;
  }

  assert(0);  // NOTREACHED
  return false;
}

bool CertificateTrust::IsDistrusted() const {
  switch (type) {
    case CertificateTrustType::DISTRUSTED:
      return true;
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
    case CertificateTrustType::TRUSTED_LEAF:
      return false;
  }

  assert(0);  // NOTREACHED
  return false;
}

bool CertificateTrust::HasUnspecifiedTrust() const {
  switch (type) {
    case CertificateTrustType::UNSPECIFIED:
      return true;
    case CertificateTrustType::DISTRUSTED:
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
    case CertificateTrustType::TRUSTED_LEAF:
      return false;
  }

  assert(0);  // NOTREACHED
  return true;
}

std::string CertificateTrust::ToDebugString() const {
  std::string result;
  switch (type) {
    case CertificateTrustType::UNSPECIFIED:
      result = "UNSPECIFIED";
      break;
    case CertificateTrustType::DISTRUSTED:
      result = "DISTRUSTED";
      break;
    case CertificateTrustType::TRUSTED_ANCHOR:
      result = "TRUSTED_ANCHOR";
      break;
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      result = "TRUSTED_ANCHOR_OR_LEAF";
      break;
    case CertificateTrustType::TRUSTED_LEAF:
      result = "TRUSTED_LEAF";
      break;
  }
  if (enforce_anchor_expiry) {
    result += "+enforce_anchor_expiry";
  }
  if (enforce_anchor_constraints) {
    result += "+enforce_anchor_constraints";
  }
  if (require_anchor_basic_constraints) {
    result += "+require_anchor_basic_constraints";
  }
  if (require_leaf_selfsigned) {
    result += "+require_leaf_selfsigned";
  }
  return result;
}

TrustStore::TrustStore() = default;

void TrustStore::AsyncGetIssuersOf(const ParsedCertificate* cert,
                                   std::unique_ptr<Request>* out_req) {
  out_req->reset();
}

}  // namespace net
