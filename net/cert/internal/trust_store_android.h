// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_
#define NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_

#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "net/base/net_export.h"
#include "net/cert/pki/trust_store.h"
#include "net/cert/pki/trust_store_in_memory.h"

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

  // Load user settings from Android.
  void Initialize();

  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;

  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) override;

 private:
  // Inner Impl class for use in initializing stores.
  class Impl;

  // Loads user settings from Windows CertStores if not already done and
  // returns pointer to the Impl.
  Impl* MaybeInitializeAndGetImpl();

  base::Lock init_lock_;
  std::unique_ptr<Impl> impl_ GUARDED_BY(init_lock_);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_ANDROID_H_
