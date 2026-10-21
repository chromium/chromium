// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SECURITY_CERTIFICATE_POLICY_CACHE_IMPL_H_
#define IOS_WEB_SECURITY_CERTIFICATE_POLICY_CACHE_IMPL_H_

#include <map>
#include <string>

#include "ios/web/public/security/certificate_policy_cache.h"
#include "ios/web/security/cert_policy.h"

namespace web {

// Private implementation of the certificate policy cache.
class CertificatePolicyCacheImpl final : public CertificatePolicyCache {
 public:
  CertificatePolicyCacheImpl();

  void AllowCertForHost(const net::X509Certificate* cert,
                        const std::string& host,
                        net::CertStatus error) override;
  Judgment QueryPolicy(const net::X509Certificate* cert,
                       const std::string& host,
                       net::CertStatus error) override;
  void ClearCertificatePolicies() override;

 private:
  ~CertificatePolicyCacheImpl() override;
  std::map<std::string, CertPolicy> cert_policy_for_host_;
};

}  // namespace web

#endif  // IOS_WEB_SECURITY_CERTIFICATE_POLICY_CACHE_IMPL_H_
