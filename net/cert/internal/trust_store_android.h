// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_
#define NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_

#include <atomic>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "net/base/net_export.h"
#include "net/cert/cert_database.h"
#include "net/cert/internal/platform_trust_store.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

namespace net {

// TrustStoreAndroid is an implementation of bssl::TrustStore which uses the
// Android cert systems to find user-added trust anchors for path building. It
// ignores the Android builtin trust anchors.
class NET_EXPORT TrustStoreAndroid : public PlatformTrustStore,
                                     public CertDatabase::Observer {
 public:
  TrustStoreAndroid();
  ~TrustStoreAndroid() override;
  TrustStoreAndroid(const TrustStoreAndroid& other) = delete;
  TrustStoreAndroid& operator=(const TrustStoreAndroid& other) = delete;

  // Load user settings from Android.
  void Initialize();

  // bssl::TrustStore:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) override;

  // net::PlatformTrustStore implementation:
  std::vector<net::PlatformTrustStore::CertWithTrust> GetAllUserAddedCerts()
      override;

  // CertDatabase::Observer:
  void OnTrustStoreChanged() override;

  // Have this object start listening for CertDatabase changes.
  // This function is not thread safe, and must be called from a sequence.
  void ObserveCertDBChanges();

 private:
  // Inner Impl class for use in initializing stores.
  class Impl;

  // Loads user settings from Windows CertStores if not already done and
  // returns scoped_refptr<Impl>.
  scoped_refptr<Impl> MaybeInitializeAndGetImpl();

  bool is_observing_certdb_changes_
      GUARDED_BY_CONTEXT(certdb_observer_sequence_checker_) = false;
  SEQUENCE_CHECKER(certdb_observer_sequence_checker_);

  base::Lock init_lock_;
  scoped_refptr<Impl> impl_ GUARDED_BY(init_lock_);
  // Generation number that is incremented whenever the backing Android trust
  // store changes.
  std::atomic_int generation_ = 0;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_
