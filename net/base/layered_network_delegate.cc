// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/layered_network_delegate.h"

#include <utility>

#include "base/memory/ptr_util.h"

namespace net {

LayeredNetworkDelegate::LayeredNetworkDelegate(
    std::unique_ptr<NetworkDelegate> nested_network_delegate)
    : owned_nested_network_delegate_(std::move(nested_network_delegate)),
      nested_network_delegate_(owned_nested_network_delegate_.get()) {}

LayeredNetworkDelegate::~LayeredNetworkDelegate() = default;

std::unique_ptr<NetworkDelegate>
LayeredNetworkDelegate::CreatePassThroughNetworkDelegate(
    NetworkDelegate* unowned_nested_network_delegate) {
  return base::WrapUnique<NetworkDelegate>(
      new LayeredNetworkDelegate(unowned_nested_network_delegate));
}

int LayeredNetworkDelegate::OnBeforeURLRequest(URLRequest* request,
                                               CompletionOnceCallback callback,
                                               GURL* new_url) {
  OnBeforeURLRequestInternal(request, new_url);
  return nested_network_delegate_->NotifyBeforeURLRequest(
      request, std::move(callback), new_url);
}

void LayeredNetworkDelegate::OnBeforeURLRequestInternal(URLRequest* request,
                                                        GURL* new_url) {}

int LayeredNetworkDelegate::OnBeforeStartTransaction(
    URLRequest* request,
    CompletionOnceCallback callback,
    HttpRequestHeaders* headers) {
  OnBeforeStartTransactionInternal(request, headers);
  return nested_network_delegate_->NotifyBeforeStartTransaction(
      request, std::move(callback), headers);
}

void LayeredNetworkDelegate::OnBeforeStartTransactionInternal(
    URLRequest* request,
    HttpRequestHeaders* headers) {}

void LayeredNetworkDelegate::OnBeforeSendHeaders(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    const ProxyRetryInfoMap& proxy_retry_info,
    HttpRequestHeaders* headers) {
  OnBeforeSendHeadersInternal(request, proxy_info, proxy_retry_info, headers);
  nested_network_delegate_->NotifyBeforeSendHeaders(request, proxy_info,
                                                    proxy_retry_info, headers);
}

void LayeredNetworkDelegate::OnBeforeSendHeadersInternal(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    const ProxyRetryInfoMap& proxy_retry_info,
    HttpRequestHeaders* headers) {}

void LayeredNetworkDelegate::OnStartTransaction(
    URLRequest* request,
    const HttpRequestHeaders& headers) {
  OnStartTransactionInternal(request, headers);
  nested_network_delegate_->NotifyStartTransaction(request, headers);
}

void LayeredNetworkDelegate::OnStartTransactionInternal(
    URLRequest* request,
    const HttpRequestHeaders& headers) {}

int LayeredNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  OnHeadersReceivedInternal(request, original_response_headers,
                            override_response_headers,
                            allowed_unsafe_redirect_url);
  return nested_network_delegate_->NotifyHeadersReceived(
      request, std::move(callback), original_response_headers,
      override_response_headers, allowed_unsafe_redirect_url);
}

void LayeredNetworkDelegate::OnHeadersReceivedInternal(
    URLRequest* request,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
}

void LayeredNetworkDelegate::OnBeforeRedirect(URLRequest* request,
                                              const GURL& new_location) {
  OnBeforeRedirectInternal(request, new_location);
  nested_network_delegate_->NotifyBeforeRedirect(request, new_location);
}

void LayeredNetworkDelegate::OnBeforeRedirectInternal(
    URLRequest* request,
    const GURL& new_location) {
}

void LayeredNetworkDelegate::OnResponseStarted(URLRequest* request,
                                               int net_error) {
  OnResponseStartedInternal(request, net_error);
  nested_network_delegate_->NotifyResponseStarted(request, net_error);
}

void LayeredNetworkDelegate::OnResponseStartedInternal(URLRequest* request,
                                                       int net_error) {}

void LayeredNetworkDelegate::OnNetworkBytesReceived(URLRequest* request,
                                                    int64_t bytes_received) {
  OnNetworkBytesReceivedInternal(request, bytes_received);
  nested_network_delegate_->NotifyNetworkBytesReceived(request, bytes_received);
}

void LayeredNetworkDelegate::OnNetworkBytesReceivedInternal(
    URLRequest* request,
    int64_t bytes_received) {}

void LayeredNetworkDelegate::OnNetworkBytesSent(URLRequest* request,
                                                int64_t bytes_sent) {
  OnNetworkBytesSentInternal(request, bytes_sent);
  nested_network_delegate_->NotifyNetworkBytesSent(request, bytes_sent);
}

void LayeredNetworkDelegate::OnNetworkBytesSentInternal(URLRequest* request,
                                                        int64_t bytes_sent) {}

void LayeredNetworkDelegate::OnCompleted(URLRequest* request,
                                         bool started,
                                         int net_error) {
  OnCompletedInternal(request, started, net_error);
  nested_network_delegate_->NotifyCompleted(request, started, net_error);
}

void LayeredNetworkDelegate::OnCompletedInternal(URLRequest* request,
                                                 bool started,
                                                 int net_error) {}

void LayeredNetworkDelegate::OnURLRequestDestroyed(URLRequest* request) {
  OnURLRequestDestroyedInternal(request);
  nested_network_delegate_->NotifyURLRequestDestroyed(request);
}

void LayeredNetworkDelegate::OnURLRequestDestroyedInternal(
    URLRequest* request) {
}

void LayeredNetworkDelegate::OnPACScriptError(int line_number,
                                              const base::string16& error) {
  OnPACScriptErrorInternal(line_number, error);
  nested_network_delegate_->NotifyPACScriptError(line_number, error);
}

void LayeredNetworkDelegate::OnPACScriptErrorInternal(
    int line_number,
    const base::string16& error) {
}

NetworkDelegate::AuthRequiredResponse LayeredNetworkDelegate::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    AuthCallback callback,
    AuthCredentials* credentials) {
  OnAuthRequiredInternal(request, auth_info, credentials);
  return nested_network_delegate_->NotifyAuthRequired(
      request, auth_info, std::move(callback), credentials);
}

void LayeredNetworkDelegate::OnAuthRequiredInternal(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    AuthCredentials* credentials) {}

bool LayeredNetworkDelegate::OnCanGetCookies(const URLRequest& request,
                                             const CookieList& cookie_list,
                                             bool allowed_from_caller) {
  return nested_network_delegate_->CanGetCookies(
      request, cookie_list,
      OnCanGetCookiesInternal(request, cookie_list, allowed_from_caller));
}

bool LayeredNetworkDelegate::OnCanGetCookiesInternal(
    const URLRequest& request,
    const CookieList& cookie_list,
    bool allowed_from_caller) {
  return allowed_from_caller;
}

bool LayeredNetworkDelegate::OnCanSetCookie(const URLRequest& request,
                                            const net::CanonicalCookie& cookie,
                                            CookieOptions* options,
                                            bool allowed_from_caller) {
  return nested_network_delegate_->CanSetCookie(
      request, cookie, options,
      OnCanSetCookieInternal(request, cookie, options, allowed_from_caller));
}

bool LayeredNetworkDelegate::OnCanSetCookieInternal(
    const URLRequest& request,
    const net::CanonicalCookie& cookie,
    CookieOptions* options,
    bool allowed_from_caller) {
  return allowed_from_caller;
}

bool LayeredNetworkDelegate::OnCanAccessFile(
    const URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  OnCanAccessFileInternal(request, original_path, absolute_path);
  return nested_network_delegate_->CanAccessFile(request, original_path,
                                                 absolute_path);
}

void LayeredNetworkDelegate::OnCanAccessFileInternal(
    const URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {}

bool LayeredNetworkDelegate::OnCanEnablePrivacyMode(
    const GURL& url,
    const GURL& site_for_cookies) const {
  return OnCanEnablePrivacyModeInternal(url, site_for_cookies) ||
         nested_network_delegate_->CanEnablePrivacyMode(url, site_for_cookies);
}

bool LayeredNetworkDelegate::OnCanEnablePrivacyModeInternal(
    const GURL& url,
    const GURL& site_for_cookies) const {
  return false;
}

bool LayeredNetworkDelegate::
    OnCancelURLRequestWithPolicyViolatingReferrerHeader(
        const URLRequest& request,
        const GURL& target_url,
        const GURL& referrer_url) const {
  return OnCancelURLRequestWithPolicyViolatingReferrerHeaderInternal(
             request, target_url, referrer_url) ||
         nested_network_delegate_
             ->CancelURLRequestWithPolicyViolatingReferrerHeader(
                 request, target_url, referrer_url);
}

bool LayeredNetworkDelegate::
    OnCancelURLRequestWithPolicyViolatingReferrerHeaderInternal(
        const URLRequest& request,
        const GURL& target_url,
        const GURL& referrer_url) const {
  return false;
}

bool LayeredNetworkDelegate::OnCanQueueReportingReport(
    const url::Origin& origin) const {
  OnCanQueueReportingReportInternal(origin);
  return nested_network_delegate_->CanQueueReportingReport(origin);
}

void LayeredNetworkDelegate::OnCanQueueReportingReportInternal(
    const url::Origin& origin) const {}

void LayeredNetworkDelegate::OnCanSendReportingReports(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  OnCanSendReportingReportsInternal(origins);
  nested_network_delegate_->CanSendReportingReports(std::move(origins),
                                                    std::move(result_callback));
}

void LayeredNetworkDelegate::OnCanSendReportingReportsInternal(
    const std::set<url::Origin>& origins) const {}

bool LayeredNetworkDelegate::OnCanSetReportingClient(
    const url::Origin& origin,
    const GURL& endpoint) const {
  OnCanSetReportingClientInternal(origin, endpoint);
  return nested_network_delegate_->CanSetReportingClient(origin, endpoint);
}

void LayeredNetworkDelegate::OnCanSetReportingClientInternal(
    const url::Origin& origin,
    const GURL& endpoint) const {}

bool LayeredNetworkDelegate::OnCanUseReportingClient(
    const url::Origin& origin,
    const GURL& endpoint) const {
  OnCanUseReportingClientInternal(origin, endpoint);
  return nested_network_delegate_->CanUseReportingClient(origin, endpoint);
}

void LayeredNetworkDelegate::OnCanUseReportingClientInternal(
    const url::Origin& origin,
    const GURL& endpoint) const {}

LayeredNetworkDelegate::LayeredNetworkDelegate(
    NetworkDelegate* unowned_nested_network_delegate)
    : nested_network_delegate_(unowned_nested_network_delegate) {}

}  // namespace net
