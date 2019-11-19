// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/ftp/ftp_request_info.h"
#include "net/ftp/ftp_transaction.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/url_request/url_request_job.h"

namespace net {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FTPStartResult : int {
  kSuccessNoAuth = 0,
  kSuccessAuth = 1,
  kFailed = 2,
  kMaxValue = kFailed
};

class NetworkDelegate;
class FtpTransactionFactory;
class FtpAuthCache;

// A URLRequestJob subclass that is built on top of FtpTransaction. It
// provides an implementation for FTP.
class NET_EXPORT_PRIVATE URLRequestFtpJob : public URLRequestJob {
 public:
  URLRequestFtpJob(URLRequest* request,
                   NetworkDelegate* network_delegate,
                   FtpTransactionFactory* ftp_transaction_factory,
                   FtpAuthCache* ftp_auth_cache);
  ~URLRequestFtpJob() override;
  void Start() override;

 protected:
  // Overridden from URLRequestJob:
  bool IsSafeRedirect(const GURL& location) override;
  bool GetMimeType(std::string* mime_type) const override;
  IPEndPoint GetResponseRemoteEndpoint() const override;
  void Kill() override;
  void GetResponseInfo(HttpResponseInfo* info) override;

 private:
  class AuthData;

  void OnResolveProxyComplete(int result);

  void StartFtpTransaction();

  void OnStartCompleted(int result);
  void OnStartCompletedAsync(int result);
  void OnReadCompleted(int result);

  void RestartTransactionWithAuth();

  // Overridden from URLRequestJob:
  LoadState GetLoadState() const override;
  bool NeedsAuth() override;
  std::unique_ptr<AuthChallengeInfo> GetAuthChallengeInfo() override;
  void SetAuth(const AuthCredentials& credentials) override;
  void CancelAuth() override;

  int ReadRawData(IOBuffer* buf, int buf_size) override;

  void HandleAuthNeededResponse();

  void LogFtpStartResult(FTPStartResult result);

  ProxyResolutionService* proxy_resolution_service_;
  ProxyInfo proxy_info_;
  std::unique_ptr<ProxyResolutionService::Request> proxy_resolve_request_;

  FtpRequestInfo ftp_request_info_;
  std::unique_ptr<FtpTransaction> ftp_transaction_;

  bool read_in_progress_;

  std::unique_ptr<AuthData> auth_data_;

  FtpTransactionFactory* ftp_transaction_factory_;
  FtpAuthCache* ftp_auth_cache_;

  base::WeakPtrFactory<URLRequestFtpJob> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(URLRequestFtpJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
