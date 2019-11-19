// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_
#define IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "ios/web/public/security/cert_policy.h"
#include "net/cert/x509_certificate.h"

namespace web {

// A manager for certificate policy decisions for hosts, used to remember
// decisions about how to handle problematic certs.
// This class is thread-safe only in that in can be created and passed around
// on any thread; the policy-related methods can only be called from the IO
// thread.
class CertificatePolicyCache
    : public base::RefCountedThreadSafe<CertificatePolicyCache> {
 public:
  // Can be called from any thread:
  CertificatePolicyCache();

  // Everything from here on can only be called from the IO thread.

  // Records that |cert| is permitted to be used for |host| in the future.
  virtual void AllowCertForHost(net::X509Certificate* cert,
                                const std::string& host,
                                net::CertStatus error);

  // Queries whether |cert| is allowed or denied for |host|.
  virtual CertPolicy::Judgment QueryPolicy(net::X509Certificate* cert,
                                           const std::string& host,
                                           net::CertStatus error);

  // Removes all policies stored in this instance.
  virtual void ClearCertificatePolicies();

 protected:
  virtual ~CertificatePolicyCache();

 private:
  friend class base::RefCountedThreadSafe<CertificatePolicyCache>;

  // Certificate policies for each host.
  std::map<std::string, CertPolicy> cert_policy_for_host_;

  DISALLOW_COPY_AND_ASSIGN(CertificatePolicyCache);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_
