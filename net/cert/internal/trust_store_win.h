// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_WIN_H_
#define NET_CERT_INTERNAL_TRUST_STORE_WIN_H_

#include <windows.h>

#include <wincrypt.h>

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/net_export.h"
#include "net/cert/internal/platform_trust_store.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

// TrustStoreWin is an implementation of bssl::TrustStore which uses the Windows
// cert systems to find user-added trust anchors for path building. It ignores
// the Windows builtin trust anchors.
//
// This bssl::TrustStore is thread-safe. The certificate stores are held in an
// immutable snapshot (`Impl`) which is never modified once it is visible to
// another thread; refreshing after a Windows store change builds a new
// snapshot and publishes it by atomic swap. Readers hold a reference to the
// snapshot for the duration of each call, so the stores they are enumerating
// stay alive even if a newer snapshot is published concurrently.
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
  std::shared_ptr<const bssl::MTCAnchor> GetTrustedMTCIssuerOf(
      const bssl::ParsedCertificate* cert) override;

  // net::PlatformTrustStore implementation:
  std::vector<net::PlatformTrustStore::CertWithTrust> GetAllUserAddedCerts()
      override;

  // Returns the Windows event that is signaled when a monitored certificate
  // store changes, or nullptr if change monitoring is not active (which is the
  // case before InitializeStores() runs, and for instances created by
  // CreateForTesting()). Signaling it drives the same refresh path Windows
  // would.
  HANDLE GetChangeEventForTesting();

  // Sets the delay used to coalesce a burst of certificate store change
  // signals into a single refresh, and returns the previous value. Only
  // affects TrustStoreWin instances that start monitoring afterwards.
  static base::TimeDelta SetChangeDebounceDelayForTesting(
      base::TimeDelta delay);

 private:
  // Immutable snapshot of the Windows certificate stores.
  class Impl;

  // Owns the Windows event that is signaled when a monitored certificate store
  // changes, and the watch for it.
  class ChangeMonitor;

  explicit TrustStoreWin(scoped_refptr<Impl> impl);

  // Returns the current snapshot, building it first if it does not exist yet
  // and rebuilding it if Windows has reported a certificate store change since
  // it was built.
  scoped_refptr<const Impl> GetImpl();

  // Serializes rebuilds and blocks accesses requiring a fresh snapshot until
  // the replacement has been published. Readers already holding a snapshot
  // can finish without acquiring this lock.
  base::Lock rebuild_lock_;

  base::Lock impl_lock_;
  scoped_refptr<const Impl> impl_ GUARDED_BY(impl_lock_);
  bool rebuild_in_progress_ GUARDED_BY(impl_lock_) = false;

  // Null until the first snapshot is built, and always null for instances
  // created by CreateForTesting(), whose stores are supplied by the test and
  // are never rebuilt.
  scoped_refptr<ChangeMonitor> monitor_ GUARDED_BY(impl_lock_);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_WIN_H_
