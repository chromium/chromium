// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_H_
#define IOS_WEB_PUBLIC_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_H_

#import <Foundation/Foundation.h>

#include "base/memory/ref_counted.h"
#include "net/cert/cert_status_flags.h"

namespace net {
class X509Certificate;
}

namespace web {

class BrowserState;
class CertificatePolicyCache;

// Stores certificate policy decisions for a specific session.  The certificate
// policy decisions stored in this object are persisted along with their
// WebStates and are used to populate the CertificatePolicyCache of a restored
// BrowserState.  Must be accessed on the UI thread.
class SessionCertificatePolicyCache {
 public:
  // |browser_state| should be non-null.
  SessionCertificatePolicyCache(BrowserState* browser_state);
  virtual ~SessionCertificatePolicyCache();

  // Transfers the allowed certificate information from this session to |cache|.
  //
  // TODO(crbug.com/1040566): Delete this method since
  // RegisterAllowedCertificate already updates the CertificatePolicyCache.
  virtual void UpdateCertificatePolicyCache(
      const scoped_refptr<web::CertificatePolicyCache>& cache) const = 0;

  // Stores certificate information that a user has indicated should be allowed
  // for this session. The web::CertificatePolicyCache will also be updated
  // when this method is called.
  virtual void RegisterAllowedCertificate(
      const scoped_refptr<net::X509Certificate> certificate,
      const std::string& host,
      net::CertStatus status) = 0;

 protected:
  // Get the web::CertificatePolicyCache for this session's BrowserState.
  // Must be called on UI thread.
  scoped_refptr<CertificatePolicyCache> GetCertificatePolicyCache() const;

 private:
  // The WebState's BrowserState used for retrieving the CertificatePolicyCache.
  BrowserState* browser_state_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SESSION_SESSION_CERTIFICATE_POLICY_CACHE_H_
