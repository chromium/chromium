// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_WIN_H_
#define NET_CERT_INTERNAL_TRUST_STORE_WIN_H_

#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
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
  // Creates a TrustStoreWin.
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

  // Loads user settings from Windows CertStores. If there are errors,
  // the underlyingTrustStoreWin object may not read all Windows
  // CertStores when making trust decisions.
  void InitializeStores();

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;

  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override;

 private:
  // Inner Impl class for use in initializing stores.
  class Impl;

  explicit TrustStoreWin(std::unique_ptr<Impl> impl);

  // Loads user settings from Windows CertStores if not already done and
  // returns pointer to the Impl.
  Impl* MaybeInitializeAndGetImpl();

  base::Lock init_lock_;
  std::unique_ptr<Impl> impl_ GUARDED_BY(init_lock_);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_WIN_H_
