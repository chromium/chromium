// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_COLLECTION_H_
#define NET_CERT_INTERNAL_TRUST_STORE_COLLECTION_H_

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/cert/internal/trust_store.h"

namespace net {

// TrustStoreCollection is an implementation of TrustStore which combines the
// results from multiple TrustStores.
//
// The order of the matches will correspond to a concatenation of matches in
// the order the stores were added.
class NET_EXPORT TrustStoreCollection : public TrustStore {
 public:
  TrustStoreCollection();
  ~TrustStoreCollection() override;

  // Includes results from |store| in the combined output. |store| must
  // outlive the TrustStoreCollection.
  void AddTrustStore(TrustStore* store);

  // TrustStore implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;
  void GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                CertificateTrust* trust,
                base::SupportsUserData* debug_data) const override;

 private:
  std::vector<TrustStore*> stores_;

  DISALLOW_COPY_AND_ASSIGN(TrustStoreCollection);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_COLLECTION_H_
