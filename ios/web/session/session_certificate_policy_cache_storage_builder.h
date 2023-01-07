// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_BUILDER_H_
#define IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_BUILDER_H_

#include <memory>

@class CRWSessionCertificatePolicyCacheStorage;

namespace web {

class BrowserState;
class SessionCertificatePolicyCacheImpl;

// Class that converts between model objects and their serializable versions.
class SessionCertificatePolicyCacheStorageBuilder {
 public:
  // Creates a CRWSessionCertificatePolicyCacheStorage from `cache`.
  static CRWSessionCertificatePolicyCacheStorage* BuildStorage(
      const SessionCertificatePolicyCacheImpl& cache);

  // Creates a SessionCertificatePolicyCache from `cache_storage`.
  static std::unique_ptr<SessionCertificatePolicyCacheImpl>
  BuildSessionCertificatePolicyCache(
      CRWSessionCertificatePolicyCacheStorage* cache_storage,
      BrowserState* browser_state);

  SessionCertificatePolicyCacheStorageBuilder() = delete;
  ~SessionCertificatePolicyCacheStorageBuilder() = delete;
};

}  // namespace web

#endif  // IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_STORAGE_BUILDER_H_
