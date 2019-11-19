// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_BUILDER_H_
#define IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_BUILDER_H_

#include <memory>

@class CRWSessionCertificatePolicyCacheStorage;

namespace web {

class SessionCertificatePolicyCacheImpl;

// Class that converts between model objects and their serializable versions.
class SessionCertificatePolicyCacheStorageBuilder {
 public:
  // Creates a CRWSessionCertificatePolicyCacheStorage from |cache|.
  CRWSessionCertificatePolicyCacheStorage* BuildStorage(
      SessionCertificatePolicyCacheImpl* cache) const;
  // Creates a SessionCertificatePolicyCache from |cache_storage|.
  std::unique_ptr<SessionCertificatePolicyCacheImpl>
  BuildSessionCertificatePolicyCache(
      CRWSessionCertificatePolicyCacheStorage* cache_storage) const;
};

}  // namespace web

#endif  // IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_BUILDER_H_
