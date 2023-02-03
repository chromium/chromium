// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_android.h"

#include "base/logging.h"
#include "net/android/network_library.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace net {

// TODO(hchao): read worker roots on a worker thread
// TODO(hchao): reload list of roots on updates to the android trust stores
TrustStoreAndroid::TrustStoreAndroid() {
  std::vector<std::string> roots = net::android::GetUserAddedRoots();

  for (auto& root : roots) {
    CertErrors errors;
    auto parsed = net::ParsedCertificate::Create(
        net::x509_util::CreateCryptoBuffer(root),
        net::x509_util::DefaultParseCertificateOptions(), &errors);
    if (!parsed) {
      LOG(ERROR) << "Error parsing certificate:\n" << errors.ToDebugString();
      continue;
    }
    trust_store_.AddTrustAnchor(parsed);
  }
}

TrustStoreAndroid::~TrustStoreAndroid() = default;

void TrustStoreAndroid::SyncGetIssuersOf(const ParsedCertificate* cert,
                                         ParsedCertificateList* issuers) {
  trust_store_.SyncGetIssuersOf(cert, issuers);
}

CertificateTrust TrustStoreAndroid::GetTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) {
  return trust_store_.GetTrust(cert, debug_data);
}

}  // namespace net
