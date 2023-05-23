// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_
#define IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/session/session_certificate_policy_cache.h"
#include "ios/web/session/session_certificate.h"

@class CRWSessionCertificateStorage;

namespace net {
class X509Certificate;
}

namespace web {

// Concrete implementation of SessionCertificatePolicyCache.
class SessionCertificatePolicyCacheImpl final
    : public SessionCertificatePolicyCache {
 public:
  explicit SessionCertificatePolicyCacheImpl(BrowserState* browser_state);
  ~SessionCertificatePolicyCacheImpl() final;

  // SessionCertificatePolicyCache:
  void UpdateCertificatePolicyCache() const final;
  void RegisterAllowedCertificate(
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::string& host,
      net::CertStatus status) final;

  // Allows for batch updating the allowed certificate storages.
  void SetAllowedCerts(NSSet<CRWSessionCertificateStorage*>* allowed_certs);
  NSSet<CRWSessionCertificateStorage*>* GetAllowedCerts() const;

 private:
  // Represents the allowed certificates.
  SessionCertificateSet allowed_certs_;
};

}  // namespace web

#endif  // IOS_WEB_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_IMPL_H_
