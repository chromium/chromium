// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_ssl_host_state_delegate.h"

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "url/gurl.h"

namespace content {

MockSSLHostStateDelegate::MockSSLHostStateDelegate() {}

MockSSLHostStateDelegate::~MockSSLHostStateDelegate() {}

void MockSSLHostStateDelegate::AllowCert(const std::string& host,
                                         const net::X509Certificate& cert,
                                         int error,
                                         StoragePartition* storage_partition) {
  exceptions_.insert(host);
}

void MockSSLHostStateDelegate::Clear(
    base::RepeatingCallback<bool(const std::string&)> host_filter) {
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
    int error,
    StoragePartition* storage_partition) {
  if (!base::Contains(exceptions_, host)) {
    return SSLHostStateDelegate::DENIED;
  }

  return SSLHostStateDelegate::ALLOWED;
}

void MockSSLHostStateDelegate::HostRanInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  hosts_ran_insecure_content_.insert(host);
}

bool MockSSLHostStateDelegate::DidHostRunInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  return base::Contains(hosts_ran_insecure_content_, host);
}

void MockSSLHostStateDelegate::AllowHttpForHost(
    const std::string& host,
    StoragePartition* storage_partition) {
  allow_http_hosts_.insert(host);
}

bool MockSSLHostStateDelegate::IsHttpAllowedForHost(
    const std::string& host,
    StoragePartition* storage_partition) {
  return base::Contains(allow_http_hosts_, host);
}

void MockSSLHostStateDelegate::SetHttpsEnforcementForHost(
    const std::string& host,
    bool enforce,
    StoragePartition* storage_partition) {
  if (enforce) {
    enforce_https_hosts_.insert(host);
  } else {
    enforce_https_hosts_.erase(host);
  }
}

bool MockSSLHostStateDelegate::IsHttpsEnforcedForUrl(
    const GURL& url,
    StoragePartition* storage_partition) {
  // HTTPS-First Mode is never auto-enabled for URLs with non-default ports.
  if (!url.port().empty()) {
    return false;
  }
  return base::Contains(enforce_https_hosts_, url.host());
}

void MockSSLHostStateDelegate::RevokeUserAllowExceptions(
    const std::string& host) {
  exceptions_.erase(host);
}

bool MockSSLHostStateDelegate::HasAllowException(
    const std::string& host,
    StoragePartition* storage_partition) {
  return base::Contains(exceptions_, host);
}

bool MockSSLHostStateDelegate::HasAllowExceptionForAnyHost(
    StoragePartition* storage_partition) {
  return !exceptions_.empty();
}

}  // namespace content
