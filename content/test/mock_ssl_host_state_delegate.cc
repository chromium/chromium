// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "content/test/mock_ssl_host_state_delegate.h"

namespace content {

MockSSLHostStateDelegate::MockSSLHostStateDelegate() {}

MockSSLHostStateDelegate::~MockSSLHostStateDelegate() {}

void MockSSLHostStateDelegate::AllowCert(const std::string& host,
                                         const net::X509Certificate& cert,
                                         int error) {
  exceptions_.insert(host);
}

void MockSSLHostStateDelegate::Clear(
    const base::Callback<bool(const std::string&)>& host_filter) {
  if (host_filter.is_null()) {
    exceptions_.clear();
  } else {
    for (auto it = exceptions_.begin(); it != exceptions_.end();) {
      auto next_it = std::next(it);

      if (host_filter.Run(*it))
        exceptions_.erase(it);

      it = next_it;
    }
  }
}

SSLHostStateDelegate::CertJudgment MockSSLHostStateDelegate::QueryPolicy(
    const std::string& host,
    const net::X509Certificate& cert,
    int error) {
  if (exceptions_.find(host) == exceptions_.end())
    return SSLHostStateDelegate::DENIED;

  return SSLHostStateDelegate::ALLOWED;
}

void MockSSLHostStateDelegate::HostRanInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {}

bool MockSSLHostStateDelegate::DidHostRunInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  return false;
}

void MockSSLHostStateDelegate::RevokeUserAllowExceptions(
    const std::string& host) {
  exceptions_.erase(exceptions_.find(host));
}

bool MockSSLHostStateDelegate::HasAllowException(const std::string& host) {
  return exceptions_.find(host) != exceptions_.end();
}

}  // namespace content
