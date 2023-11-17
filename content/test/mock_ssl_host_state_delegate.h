// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_SSL_HOST_STATE_DELEGATE_H_
#define CONTENT_TEST_MOCK_SSL_HOST_STATE_DELEGATE_H_

#include "content/public/browser/ssl_host_state_delegate.h"

#include <set>
#include <string>

namespace content {

class MockSSLHostStateDelegate : public SSLHostStateDelegate {
 public:
  MockSSLHostStateDelegate();
  ~MockSSLHostStateDelegate() override;

  void AllowCert(const std::string& host,
                 const net::X509Certificate& cert,
                 int error,
                 StoragePartition* storage_partition) override;

  void Clear(
      base::RepeatingCallback<bool(const std::string&)> host_filter) override;

  CertJudgment QueryPolicy(const std::string& host,
                           const net::X509Certificate& cert,
                           int error,
                           StoragePartition* storage_partition) override;

  void HostRanInsecureContent(const std::string& host,
                              int child_id,
                              InsecureContentType content_type) override;

  bool DidHostRunInsecureContent(const std::string& host,
                                 int child_id,
                                 InsecureContentType content_type) override;

  void AllowHttpForHost(const std::string& host,
                        StoragePartition* storage_partition) override;
  bool IsHttpAllowedForHost(const std::string& host,
                            StoragePartition* storage_partition) override;

  void SetHttpsEnforcementForHost(const std::string& host,
                                  bool enforce,
                                  StoragePartition* storage_partition) override;
  bool IsHttpsEnforcedForUrl(const GURL& url,
                             StoragePartition* storage_partition) override;

  void RevokeUserAllowExceptions(const std::string& host) override;

  bool HasAllowException(const std::string& host,
                         StoragePartition* storage_partition) override;

  bool HasAllowExceptionForAnyHost(
      StoragePartition* storage_partition) override;

 private:
  std::set<std::string> exceptions_;
  std::set<std::string> hosts_ran_insecure_content_;
  std::set<std::string> allow_http_hosts_;
  std::set<std::string> enforce_https_hosts_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_SSL_HOST_STATE_DELEGATE_H_
