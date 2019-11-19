// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_collection.h"

namespace net {

TrustStoreCollection::TrustStoreCollection() = default;
TrustStoreCollection::~TrustStoreCollection() = default;

void TrustStoreCollection::AddTrustStore(TrustStore* store) {
  DCHECK(store);
  stores_.push_back(store);
}

void TrustStoreCollection::SyncGetIssuersOf(const ParsedCertificate* cert,
                                            ParsedCertificateList* issuers) {
  for (auto* store : stores_) {
    store->SyncGetIssuersOf(cert, issuers);
  }
}

void TrustStoreCollection::GetTrust(
    const scoped_refptr<ParsedCertificate>& cert,
    CertificateTrust* out_trust,
    base::SupportsUserData* debug_data) const {
  // The current aggregate result.
  CertificateTrust result = CertificateTrust::ForUnspecified();

  for (auto* store : stores_) {
    CertificateTrust cur_trust;
    store->GetTrust(cert, &cur_trust, debug_data);

    // * If any stores distrust the certificate, consider it untrusted.
    // * If multiple stores consider it trusted, use the trust result from the
    //   last one
    if (!cur_trust.HasUnspecifiedTrust()) {
      result = cur_trust;
      if (result.IsDistrusted())
        break;
    }
  }

  *out_trust = result;
}

}  // namespace net
