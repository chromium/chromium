// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_in_memory.h"

namespace net {

TrustStoreInMemory::TrustStoreInMemory() = default;
TrustStoreInMemory::~TrustStoreInMemory() = default;

void TrustStoreInMemory::Clear() {
  entries_.clear();
}

void TrustStoreInMemory::AddTrustAnchor(scoped_refptr<ParsedCertificate> cert) {
  AddCertificate(std::move(cert), CertificateTrust::ForTrustAnchor());
}

void TrustStoreInMemory::AddTrustAnchorWithConstraints(
    scoped_refptr<ParsedCertificate> cert) {
  AddCertificate(std::move(cert),
                 CertificateTrust::ForTrustAnchorEnforcingConstraints());
}

void TrustStoreInMemory::AddDistrustedCertificateForTest(
    scoped_refptr<ParsedCertificate> cert) {
  AddCertificate(std::move(cert), CertificateTrust::ForDistrusted());
}

void TrustStoreInMemory::AddCertificateWithUnspecifiedTrust(
    scoped_refptr<ParsedCertificate> cert) {
  AddCertificate(std::move(cert), CertificateTrust::ForUnspecified());
}

void TrustStoreInMemory::SyncGetIssuersOf(const ParsedCertificate* cert,
                                          ParsedCertificateList* issuers) {
  auto range = entries_.equal_range(cert->normalized_issuer().AsStringPiece());
  for (auto it = range.first; it != range.second; ++it)
    issuers->push_back(it->second.cert);
}

void TrustStoreInMemory::GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                                  CertificateTrust* trust,
                                  base::SupportsUserData* debug_data) const {
  auto range = entries_.equal_range(cert->normalized_subject().AsStringPiece());
  for (auto it = range.first; it != range.second; ++it) {
    if (cert.get() == it->second.cert.get() ||
        cert->der_cert() == it->second.cert->der_cert()) {
      *trust = it->second.trust;
      // NOTE: ambiguity when there are duplicate entries.
      return;
    }
  }
  *trust = CertificateTrust::ForUnspecified();
}

bool TrustStoreInMemory::Contains(const ParsedCertificate* cert) const {
  for (const auto& it : entries_) {
    if (cert->der_cert() == it.second.cert->der_cert())
      return true;
  }
  return false;
}

TrustStoreInMemory::Entry::Entry() = default;
TrustStoreInMemory::Entry::Entry(const Entry& other) = default;
TrustStoreInMemory::Entry::~Entry() = default;

void TrustStoreInMemory::AddCertificate(scoped_refptr<ParsedCertificate> cert,
                                        const CertificateTrust& trust) {
  Entry entry;
  entry.cert = std::move(cert);
  entry.trust = trust;

  // TODO(mattm): should this check for duplicate certificates?
  entries_.insert(
      std::make_pair(entry.cert->normalized_subject().AsStringPiece(), entry));
}

}  // namespace net
