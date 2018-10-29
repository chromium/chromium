// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate_impl.h"

#include "net/base/net_errors.h"

namespace net {

int NetworkDelegateImpl::OnBeforeURLRequest(URLRequest* request,
                                            CompletionOnceCallback callback,
                                            GURL* new_url) {
  return OK;
}

int NetworkDelegateImpl::OnBeforeStartTransaction(
    URLRequest* request,
    CompletionOnceCallback callback,
    HttpRequestHeaders* headers) {
  return OK;
}

void NetworkDelegateImpl::OnBeforeSendHeaders(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    const ProxyRetryInfoMap& proxy_retry_info,
    HttpRequestHeaders* headers) {}

void NetworkDelegateImpl::OnStartTransaction(
    URLRequest* request,
    const HttpRequestHeaders& headers) {}

int NetworkDelegateImpl::OnHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return OK;
}

void NetworkDelegateImpl::OnBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {}

void NetworkDelegateImpl::OnResponseStarted(URLRequest* request,
                                            int net_error) {}

void NetworkDelegateImpl::OnNetworkBytesReceived(URLRequest* request,
                                                 int64_t bytes_received) {}

void NetworkDelegateImpl::OnNetworkBytesSent(URLRequest* request,
                                             int64_t bytes_sent) {}

void NetworkDelegateImpl::OnCompleted(URLRequest* request,
                                      bool started,
                                      int net_error) {}

void NetworkDelegateImpl::OnURLRequestDestroyed(URLRequest* request) {
}

void NetworkDelegateImpl::OnPACScriptError(int line_number,
                                           const base::string16& error) {
}

NetworkDelegate::AuthRequiredResponse NetworkDelegateImpl::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    AuthCallback callback,
    AuthCredentials* credentials) {
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool NetworkDelegateImpl::OnCanGetCookies(const URLRequest& request,
                                          const CookieList& cookie_list,
                                          bool allowed_from_caller) {
  return allowed_from_caller;
}

bool NetworkDelegateImpl::OnCanSetCookie(const URLRequest& request,
                                         const net::CanonicalCookie& cookie,
                                         CookieOptions* options,
                                         bool allowed_from_caller) {
  return allowed_from_caller;
}

bool NetworkDelegateImpl::OnCanAccessFile(
    const URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return false;
}

bool NetworkDelegateImpl::OnCanEnablePrivacyMode(
    const GURL& url,
    const GURL& site_for_cookies) const {
  return false;
}

bool NetworkDelegateImpl::OnCancelURLRequestWithPolicyViolatingReferrerHeader(
    const URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  return false;
}

bool NetworkDelegateImpl::OnCanQueueReportingReport(
    const url::Origin& origin) const {
  return true;
}

void NetworkDelegateImpl::OnCanSendReportingReports(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  std::move(result_callback).Run(std::move(origins));
}

bool NetworkDelegateImpl::OnCanSetReportingClient(const url::Origin& origin,
                                                  const GURL& endpoint) const {
  return true;
}

bool NetworkDelegateImpl::OnCanUseReportingClient(const url::Origin& origin,
                                                  const GURL& endpoint) const {
  return true;
}

}  // namespace net
