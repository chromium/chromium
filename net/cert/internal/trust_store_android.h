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
#include "net/cert/pki/trust_store.h"
#include "net/cert/pki/trust_store_in_memory.h"

namespace net {

// TrustStoreAndroid is an implementation of TrustStore which uses the Android
// cert systems to find user-added trust anchors for path building. It ignores
// the Android builtin trust anchors.
class NET_EXPORT TrustStoreAndroid : public TrustStore,
                                     public CertDatabase::Observer {
 public:
  TrustStoreAndroid();
  ~TrustStoreAndroid() override;
  TrustStoreAndroid(const TrustStoreAndroid& other) = delete;
  TrustStoreAndroid& operator=(const TrustStoreAndroid& other) = delete;

  // Load user settings from Android.
  void Initialize();

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;

  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override;

  // CertDatabase::Observer:
  void OnCertDBChanged() override;

  // Have this object start listening for CertDB changes.
  // This function is not thread safe, and must be called from a sequence.
  void ObserveCertDBChanges();

 private:
  bool is_observing_certdb_changes = false;

  // Inner Impl class for use in initializing stores.
  class Impl;

  // Loads user settings from Windows CertStores if not already done and
  // returns scoped_refptr<Impl>.
  scoped_refptr<Impl> MaybeInitializeAndGetImpl();

  base::Lock init_lock_;
  scoped_refptr<Impl> impl_ GUARDED_BY(init_lock_);
  // Generation number that is incremented whenever the backing Android trust
  // store changes.
  std::atomic_int generation_ = 0;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_
