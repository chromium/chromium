// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_WIN_H_
#define NET_CERT_INTERNAL_TRUST_STORE_WIN_H_

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/net_export.h"
#include "net/cert/internal/trust_store.h"

namespace net {

// TrustStoreWin is an implementation of TrustStore which uses the Windows cert
// systems to find user-added trust anchors for path building. It ignores the
// Windows builtin trust anchors. This TrustStore is thread-safe (we think).
// TODO(https://crbug.com/1239270): confirm this is thread safe.
class NET_EXPORT TrustStoreWin : public TrustStore {
 public:
  ~TrustStoreWin() override;
  TrustStoreWin(const TrustStoreWin& other) = delete;
  TrustStoreWin& operator=(const TrustStoreWin& other) = delete;

  // Creates a TrustStoreWin by reading user settings from Windows system
  // CertStores. Returns nullptr on failure.
  static std::unique_ptr<TrustStoreWin> Create();

  // Creates a TrustStoreWin for testing, which will treat `root_cert_store`
  // as if it's the source of truth for roots for `GetTrust,
  // and `all_certs_store` as the store for locating certificates during
  // `SyncGetIssuersOf`.
  static std::unique_ptr<TrustStoreWin> CreateForTesting(
      crypto::ScopedHCERTSTORE root_cert_store,
      crypto::ScopedHCERTSTORE all_certs_store);

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;

  void GetTrust(const scoped_refptr<ParsedCertificate>& cert,
                CertificateTrust* trust,
                base::SupportsUserData* debug_data) const override;

 private:
  TrustStoreWin(crypto::ScopedHCERTSTORE root_cert_store,
                crypto::ScopedHCERTSTORE all_certs_store);

  // Cert Collection containing all user-added trust anchors.
  crypto::ScopedHCERTSTORE root_cert_store_;

  // Cert Collection for searching via SyncGetIssuersOf()
  crypto::ScopedHCERTSTORE all_certs_store_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_WIN_H_