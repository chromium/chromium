// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_
#define IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_

#include <map>
#include <string>

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

  CertificatePolicyCache(const CertificatePolicyCache&) = delete;
  CertificatePolicyCache& operator=(const CertificatePolicyCache&) = delete;

  // Everything from here on can only be called from the IO thread.

  // Records that `cert` is permitted to be used for `host` in the future.
  void AllowCertForHost(const net::X509Certificate* cert,
                        const std::string& host,
                        net::CertStatus error);

  // Queries whether `cert` is allowed or denied for `host`.
  CertPolicy::Judgment QueryPolicy(const net::X509Certificate* cert,
                                   const std::string& host,
                                   net::CertStatus error);

  // Removes all policies stored in this instance.
  void ClearCertificatePolicies();

 private:
  friend class base::RefCountedThreadSafe<CertificatePolicyCache>;

  ~CertificatePolicyCache();

  // Certificate policies for each host.
  std::map<std::string, CertPolicy> cert_policy_for_host_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_SECURITY_CERTIFICATE_POLICY_CACHE_H_
