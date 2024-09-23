// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_WIN_H_
#define NET_CERT_INTERNAL_TRUST_STORE_WIN_H_

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/net_export.h"
#include "net/cert/internal/platform_trust_store.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

// TrustStoreWin is an implementation of bssl::TrustStore which uses the Windows
// cert systems to find user-added trust anchors for path building. It ignores
// the Windows builtin trust anchors. This bssl::TrustStore is thread-safe (we
// think).
// TODO(crbug.com/40784682): confirm this is thread safe.
class NET_EXPORT TrustStoreWin : public PlatformTrustStore {
 public:
  struct NET_EXPORT_PRIVATE CertStores {
    ~CertStores();
    CertStores(CertStores&& other);
    CertStores& operator=(CertStores&& other);

    // Create a CertStores object with the stores initialized with (empty)
    // CERT_STORE_PROV_COLLECTION stores.
    static CertStores CreateWithCollections();

    // Create a CertStores object with the stores pre-initialized with
    // in-memory cert stores for testing purposes.
    static CertStores CreateInMemoryStoresForTesting();

    // Create a CertStores object with null cert store pointers for testing
    // purposes.
    static CertStores CreateNullStoresForTesting();

    // Returns true if any of the cert stores are not initialized.
    bool is_null() const {
      return !roots.get() || !intermediates.get() || !trusted_people.get() ||
             !disallowed.get() || !all.get();
    }

    crypto::ScopedHCERTSTORE roots;
    crypto::ScopedHCERTSTORE intermediates;
    crypto::ScopedHCERTSTORE trusted_people;
    crypto::ScopedHCERTSTORE disallowed;
    crypto::ScopedHCERTSTORE all;

   private:
    CertStores();

    void InitializeAllCertsStore();
  };

  // Creates a TrustStoreWin.
  TrustStoreWin();

  ~TrustStoreWin() override;
  TrustStoreWin(const TrustStoreWin& other) = delete;
  TrustStoreWin& operator=(const TrustStoreWin& other) = delete;

  // Creates a TrustStoreWin for testing, which will treat `root_cert_store`
  // as if it's the source of truth for roots for `GetTrust,
  // and `intermediate_cert_store` as an extra store (in addition to
  // root_cert_store) for locating certificates during `SyncGetIssuersOf`.
  static std::unique_ptr<TrustStoreWin> CreateForTesting(CertStores stores);

  // Loads user settings from Windows CertStores. If there are errors,
  // the underlyingTrustStoreWin object may not read all Windows
  // CertStores when making trust decisions.
  void InitializeStores();

  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;

  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) override;

  // net::PlatformTrustStore implementation:
  std::vector<net::PlatformTrustStore::CertWithTrust> GetAllUserAddedCerts()
      override;

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
