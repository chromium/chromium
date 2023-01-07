// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/session/session_certificate_policy_cache_storage_builder.h"

#import <Foundation/Foundation.h>

#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// static
CRWSessionCertificatePolicyCacheStorage*
SessionCertificatePolicyCacheStorageBuilder::BuildStorage(
    const SessionCertificatePolicyCacheImpl& cache) {
  CRWSessionCertificatePolicyCacheStorage* storage =
      [[CRWSessionCertificatePolicyCacheStorage alloc] init];
  storage.certificateStorages = [NSSet setWithSet:cache.GetAllowedCerts()];
  return storage;
}

// static
std::unique_ptr<SessionCertificatePolicyCacheImpl>
SessionCertificatePolicyCacheStorageBuilder::BuildSessionCertificatePolicyCache(
    CRWSessionCertificatePolicyCacheStorage* cache_storage,
    BrowserState* browser_state) {
  std::unique_ptr<SessionCertificatePolicyCacheImpl> cache =
      std::make_unique<SessionCertificatePolicyCacheImpl>(browser_state);
  cache->SetAllowedCerts(cache_storage.certificateStorages);
  return cache;
}

}  // namespace web
