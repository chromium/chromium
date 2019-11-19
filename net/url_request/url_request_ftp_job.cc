// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_ftp_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_auth_cache.h"
#include "net/ftp/ftp_network_transaction.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"

namespace net {

class URLRequestFtpJob::AuthData {
 public:
  AuthState state;  // Whether we need, have, or gave up on authentication.
  AuthCredentials credentials;  // The credentials to use for auth.

  AuthData();
  ~AuthData();
};

URLRequestFtpJob::AuthData::AuthData() : state(AUTH_STATE_NEED_AUTH) {}

URLRequestFtpJob::AuthData::~AuthData() = default;

URLRequestFtpJob::URLRequestFtpJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    FtpTransactionFactory* ftp_transaction_factory,
    FtpAuthCache* ftp_auth_cache)
    : URLRequestJob(request, network_delegate),
      proxy_resolution_service_(
          request_->context()->proxy_resolution_service()),
      read_in_progress_(false),
      ftp_transaction_factory_(ftp_transaction_factory),
      ftp_auth_cache_(ftp_auth_cache) {
  DCHECK(proxy_resolution_service_);
  DCHECK(ftp_transaction_factory);
  DCHECK(ftp_auth_cache);
}

URLRequestFtpJob::~URLRequestFtpJob() {
  Kill();
}

bool URLRequestFtpJob::IsSafeRedirect(const GURL& location) {
  // Disallow all redirects.
  return false;
}

bool URLRequestFtpJob::GetMimeType(std::string* mime_type) const {
  // When auth has been cancelled, return a blank text/plain page instead of
  // triggering a download.
  if (auth_data_ && auth_data_->state == AUTH_STATE_CANCELED) {
    *mime_type = "text/plain";
    return true;
  }

  if (ftp_transaction_->GetResponseInfo()->is_directory_listing) {
    *mime_type = "text/vnd.chromium.ftp-dir";
    return true;
  }

  // FTP resources other than directory listings ought to be handled as raw
  // binary data, not sniffed into HTML or etc.
  *mime_type = "application/octet-stream";
  return true;
}

IPEndPoint URLRequestFtpJob::GetResponseRemoteEndpoint() const {
  if (!ftp_transaction_)
    return IPEndPoint();
  return ftp_transaction_->GetResponseInfo()->remote_endpoint;
}

void URLRequestFtpJob::Start() {
  DCHECK(!proxy_resolve_request_);
  DCHECK(!ftp_transaction_);

  int rv = OK;
  if (request_->load_flags() & LOAD_BYPASS_PROXY) {
    proxy_info_.UseDirect();
  } else {
    DCHECK_EQ(request_->context()->proxy_resolution_service(),
              proxy_resolution_service_);
    // "Fine" to use an empty NetworkIsolationKey() because FTP is slated for
    // removal.
    rv = proxy_resolution_service_->ResolveProxy(
        request_->url(), "GET", NetworkIsolationKey(), &proxy_info_,
        base::BindOnce(&URLRequestFtpJob::OnResolveProxyComplete,
                       base::Unretained(this)),
        &proxy_resolve_request_, request_->net_log());

    if (rv == ERR_IO_PENDING)
      return;
  }
  OnResolveProxyComplete(rv);
}

void URLRequestFtpJob::Kill() {
  if (proxy_resolve_request_) {
    proxy_resolve_request_.reset();
  }
  if (ftp_transaction_)
    ftp_transaction_.reset();
  URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

void URLRequestFtpJob::GetResponseInfo(HttpResponseInfo* info) {
  // Don't expose the challenge if it has already been successfully
  // authenticated.
  if (!auth_data_ || auth_data_->state == AUTH_STATE_HAVE_AUTH)
    return;

  std::unique_ptr<AuthChallengeInfo> challenge = GetAuthChallengeInfo();
  if (challenge)
    info->auth_challenge = *challenge;
}

void URLRequestFtpJob::OnResolveProxyComplete(int result) {
  proxy_resolve_request_ = nullptr;

  if (result != OK) {
    OnStartCompletedAsync(result);
    return;
  }

  // Remove unsupported proxies from the list.
  proxy_info_.RemoveProxiesWithoutScheme(ProxyServer::SCHEME_DIRECT);

  if (proxy_info_.is_direct()) {
    StartFtpTransaction();
  } else {
    OnStartCompletedAsync(ERR_NO_SUPPORTED_PROXIES);
  }
}

void URLRequestFtpJob::StartFtpTransaction() {
  // Create a transaction.
  DCHECK(!ftp_transaction_);

  ftp_request_info_.url = request_->url();
  ftp_transaction_ = ftp_transaction_factory_->CreateTransaction();

  int rv;
  if (ftp_transaction_) {
    rv = ftp_transaction_->Start(
        &ftp_request_info_,
        base::BindOnce(&URLRequestFtpJob::OnStartCompleted,
                       base::Unretained(this)),
        request_->net_log(), request_->traffic_annotation());
    if (rv == ERR_IO_PENDING)
      return;
  } else {
    rv = ERR_FAILED;
  }
  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  OnStartCompletedAsync(rv);
}

void URLRequestFtpJob::OnStartCompleted(int result) {
  if (result == OK) {
    DCHECK(ftp_transaction_);

    // FTP obviously doesn't have HTTP Content-Length header. We have to pass
    // the content size information manually.
    set_expected_content_size(
        ftp_transaction_->GetResponseInfo()->expected_content_size);

    if (auth_data_ && auth_data_->state == AUTH_STATE_HAVE_AUTH) {
      LogFtpStartResult(FTPStartResult::kSuccessAuth);
    } else {
      LogFtpStartResult(FTPStartResult::kSuccessNoAuth);
    }

    NotifyHeadersComplete();
  } else if (ftp_transaction_ /* May be null if creation fails. */ &&
             ftp_transaction_->GetResponseInfo()->needs_auth) {
    HandleAuthNeededResponse();
  } else {
    LogFtpStartResult(FTPStartResult::kFailed);
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestFtpJob::OnStartCompletedAsync(int result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestFtpJob::OnStartCompleted,
                                weak_factory_.GetWeakPtr(), result));
}

void URLRequestFtpJob::OnReadCompleted(int result) {
  read_in_progress_ = false;
  ReadRawDataComplete(result);
}

void URLRequestFtpJob::RestartTransactionWithAuth() {
  DCHECK(auth_data_.get() && auth_data_->state == AUTH_STATE_HAVE_AUTH);

  int rv = ftp_transaction_->RestartWithAuth(
      auth_data_->credentials,
      base::BindOnce(&URLRequestFtpJob::OnStartCompleted,
                     base::Unretained(this)));
  if (rv == ERR_IO_PENDING)
    return;

  OnStartCompletedAsync(rv);
}

LoadState URLRequestFtpJob::GetLoadState() const {
  if (proxy_resolve_request_)
    return proxy_resolve_request_->GetLoadState();
  return ftp_transaction_ ? ftp_transaction_->GetLoadState() : LOAD_STATE_IDLE;
}

bool URLRequestFtpJob::NeedsAuth() {
  return auth_data_.get() && auth_data_->state == AUTH_STATE_NEED_AUTH;
}

std::unique_ptr<AuthChallengeInfo> URLRequestFtpJob::GetAuthChallengeInfo() {
  std::unique_ptr<AuthChallengeInfo> result =
      std::make_unique<AuthChallengeInfo>();
  result->is_proxy = false;
  result->challenger = url::Origin::Create(request_->url());
  // scheme, realm, path, and challenge are kept empty.
  DCHECK(result->scheme.empty());
  DCHECK(result->realm.empty());
  DCHECK(result->challenge.empty());
  DCHECK(result->path.empty());
  return result;
}

void URLRequestFtpJob::SetAuth(const AuthCredentials& credentials) {
  DCHECK(ftp_transaction_);
  DCHECK(NeedsAuth());

  auth_data_->state = AUTH_STATE_HAVE_AUTH;
  auth_data_->credentials = credentials;

  ftp_auth_cache_->Add(request_->url().GetOrigin(), auth_data_->credentials);

  RestartTransactionWithAuth();
}

void URLRequestFtpJob::CancelAuth() {
  DCHECK(ftp_transaction_);
  DCHECK(NeedsAuth());

  auth_data_->state = AUTH_STATE_CANCELED;

  ftp_transaction_.reset();
  NotifyHeadersComplete();
}

int URLRequestFtpJob::ReadRawData(IOBuffer* buf, int buf_size) {
  DCHECK_NE(buf_size, 0);
  DCHECK(!read_in_progress_);

  if (!ftp_transaction_)
    return 0;

  int rv =
      ftp_transaction_->Read(buf, buf_size,
                             base::BindOnce(&URLRequestFtpJob::OnReadCompleted,
                                            base::Unretained(this)));

  if (rv == ERR_IO_PENDING)
    read_in_progress_ = true;
  return rv;
}

void URLRequestFtpJob::HandleAuthNeededResponse() {
  GURL origin = request_->url().GetOrigin();

  if (auth_data_.get()) {
    if (auth_data_->state == AUTH_STATE_CANCELED) {
      NotifyHeadersComplete();
      return;
    }

    if (ftp_transaction_ && auth_data_->state == AUTH_STATE_HAVE_AUTH) {
      ftp_auth_cache_->Remove(origin, auth_data_->credentials);

      // The user entered invalid auth
      LogFtpStartResult(FTPStartResult::kFailed);
    }
  } else {
    auth_data_ = std::make_unique<AuthData>();
  }
  auth_data_->state = AUTH_STATE_NEED_AUTH;

  FtpAuthCache::Entry* cached_auth = nullptr;
  if (ftp_transaction_ && ftp_transaction_->GetResponseInfo()->needs_auth)
    cached_auth = ftp_auth_cache_->Lookup(origin);
  if (cached_auth) {
    // Retry using cached auth data.
    SetAuth(cached_auth->credentials);
  } else {
    // Prompt for a username/password.
    NotifyHeadersComplete();
  }
}

void URLRequestFtpJob::LogFtpStartResult(FTPStartResult result) {
  UMA_HISTOGRAM_ENUMERATION("Net.FTP.StartResult", result);
}

}  // namespace net
