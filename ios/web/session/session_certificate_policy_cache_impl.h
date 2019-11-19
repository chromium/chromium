// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_
#define IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/session/session_certificate_policy_cache.h"

namespace net {
class X509Certificate;
}

namespace web {

// Concrete implementation of SessionCertificatePolicyCache.
class SessionCertificatePolicyCacheImpl : public SessionCertificatePolicyCache {
 public:
  SessionCertificatePolicyCacheImpl();
  ~SessionCertificatePolicyCacheImpl() override;

  // SessionCertificatePolicyCache:
  void UpdateCertificatePolicyCache(
      const scoped_refptr<web::CertificatePolicyCache>& cache) const override;

  // Stores certificate information that a user has indicated should be allowed
  // for this session.
  void RegisterAllowedCertificate(
      const scoped_refptr<net::X509Certificate> certificate,
      const std::string& host,
      net::CertStatus status);

  // Allows for batch updating the allowed certificate storages.
  void SetAllowedCerts(NSSet* allowed_certs);
  NSSet* GetAllowedCerts() const;

 private:
  // An set of CRWSessionCertificateStorages representing allowed certs.
  NSMutableSet* allowed_certs_;

  DISALLOW_COPY_AND_ASSIGN(SessionCertificatePolicyCacheImpl);
};

}  // namespace web

#endif  // IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_
