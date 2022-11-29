// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_WIN_H_
#define NET_CERT_INTERNAL_TRUST_STORE_WIN_H_

#include "base/memory/ptr_util.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/net_export.h"
#include "net/cert/pki/trust_store.h"

namespace net {

// TrustStoreWin is an implementation of TrustStore which uses the Windows cert
// systems to find user-added trust anchors for path building. It ignores the
// Windows builtin trust anchors. This TrustStore is thread-safe (we think).
// TODO(https://crbug.com/1239270): confirm this is thread safe.
class NET_EXPORT TrustStoreWin : public TrustStore {
 public:
  // Creates a TrustStoreWin by reading user settings from Windows system
  // CertStores. If there are errors, will return a TrustStoreWin object
  // that may not read all Windows system CertStores.
  TrustStoreWin();

  ~TrustStoreWin() override;
  TrustStoreWin(const TrustStoreWin& other) = delete;
  TrustStoreWin& operator=(const TrustStoreWin& other) = delete;

  // Creates a TrustStoreWin for testing, which will treat `root_cert_store`
  // as if it's the source of truth for roots for `GetTrust,
  // and `intermediate_cert_store` as an extra store (in addition to
  // root_cert_store) for locating certificates during `SyncGetIssuersOf`.
  static std::unique_ptr<TrustStoreWin> CreateForTesting(
      crypto::ScopedHCERTSTORE root_cert_store,
      crypto::ScopedHCERTSTORE intermediate_cert_store,
      crypto::ScopedHCERTSTORE disallowed_cert_store);

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;

  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) const override;

 private:
  TrustStoreWin(crypto::ScopedHCERTSTORE root_cert_store,
                crypto::ScopedHCERTSTORE intermediate_cert_store,
                crypto::ScopedHCERTSTORE disallowed_cert_store,
                crypto::ScopedHCERTSTORE all_certs_store);

  // Cert Collection containing all user-added trust anchors.
  crypto::ScopedHCERTSTORE root_cert_store_;

  // Cert Collection containing all user-added intermediates.
  crypto::ScopedHCERTSTORE intermediate_cert_store_;

  // Cert Collection for searching via SyncGetIssuersOf()
  crypto::ScopedHCERTSTORE all_certs_store_;

  // Cert Collection for all disallowed certs.
  crypto::ScopedHCERTSTORE disallowed_cert_store_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_WIN_H_
