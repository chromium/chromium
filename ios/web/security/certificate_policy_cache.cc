// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/security/certificate_policy_cache.h"

#include "base/check_op.h"
#include "ios/web/public/thread/web_thread.h"

namespace web {

CertificatePolicyCache::CertificatePolicyCache() {}

CertificatePolicyCache::~CertificatePolicyCache() {}

void CertificatePolicyCache::AllowCertForHost(const net::X509Certificate* cert,
                                              const std::string& host,
                                              net::CertStatus error) {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  cert_policy_for_host_[host].Allow(cert, error);
}

CertPolicy::Judgment CertificatePolicyCache::QueryPolicy(
    const net::X509Certificate* cert,
    const std::string& host,
    net::CertStatus error) {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  return cert_policy_for_host_[host].Check(cert, error);
}

void CertificatePolicyCache::ClearCertificatePolicies() {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  cert_policy_for_host_.clear();
}

}  // namespace web
