// Copyright 2017 The Chromium Authors
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
  SessionCertificatePolicyCacheImpl(BrowserState* browser_state);

  SessionCertificatePolicyCacheImpl(const SessionCertificatePolicyCacheImpl&) =
      delete;
  SessionCertificatePolicyCacheImpl& operator=(
      const SessionCertificatePolicyCacheImpl&) = delete;

  ~SessionCertificatePolicyCacheImpl() override;

  // SessionCertificatePolicyCache:
  void UpdateCertificatePolicyCache(
      const scoped_refptr<web::CertificatePolicyCache>& cache) const override;
  void RegisterAllowedCertificate(
      scoped_refptr<net::X509Certificate> certificate,
      const std::string& host,
      net::CertStatus status) override;

  // Allows for batch updating the allowed certificate storages.
  void SetAllowedCerts(NSSet* allowed_certs);
  NSSet* GetAllowedCerts() const;

 private:
  // An set of CRWSessionCertificateStorages representing allowed certs.
  NSMutableSet* allowed_certs_;
};

}  // namespace web

#endif  // IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_
