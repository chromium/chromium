// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_android.h"

#include "net/cert/pki/parsed_certificate.h"

namespace net {

TrustStoreAndroid::TrustStoreAndroid() = default;

TrustStoreAndroid::~TrustStoreAndroid() = default;

void TrustStoreAndroid::SyncGetIssuersOf(const ParsedCertificate* cert,
                                         ParsedCertificateList* issuers) {
  // TODO(crbug.com/1365571): Implement looking at user added trust
  // anchors/intermediates/distrusted anchors.
}

CertificateTrust TrustStoreAndroid::GetTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) {
  // TODO(crbug.com/1365571): Implement looking at user added trust
  // anchors/intermediates/distrusted anchors.
  return CertificateTrust::ForUnspecified();
}

}  // namespace net
