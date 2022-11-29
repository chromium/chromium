// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_
#define NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_

#include "net/base/net_export.h"
#include "net/cert/pki/trust_store.h"

namespace net {

// TrustStoreAndroid is an implementation of TrustStore which uses the Android
// cert systems to find user-added trust anchors for path building. It ignores
// the Android builtin trust anchors.
class NET_EXPORT TrustStoreAndroid : public TrustStore {
 public:
  TrustStoreAndroid();
  ~TrustStoreAndroid() override;
  TrustStoreAndroid(const TrustStoreAndroid& other) = delete;
  TrustStoreAndroid& operator=(const TrustStoreAndroid& other) = delete;

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;

  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) const override;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_
