// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate.h"

#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/url_request/url_request.h"

namespace net {

NetworkDelegate::~NetworkDelegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

int NetworkDelegate::NotifyBeforeURLRequest(URLRequest* request,
                                            CompletionOnceCallback callback,
                                            GURL* new_url) {
  TRACE_EVENT0(NetTracingCategory(), "NetworkDelegate::NotifyBeforeURLRequest");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(request);
  DCHECK(!callback.is_null());

  // ClusterFuzz depends on the following VLOG. See: crbug.com/715656
  VLOG(1) << "NetworkDelegate::NotifyBeforeURLRequest: " << request->url();
  return OnBeforeURLRequest(request, std::move(callback), new_url);
}

int NetworkDelegate::NotifyBeforeStartTransaction(
    URLRequest* request,
    CompletionOnceCallback callback,
    HttpRequestHeaders* headers) {
  TRACE_EVENT0(NetTracingCategory(),
               "NetworkDelegate::NotifyBeforeStartTransation");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(headers);
  DCHECK(!callback.is_null());
  return OnBeforeStartTransaction(request, std::move(callback), headers);
}

void NetworkDelegate::NotifyBeforeSendHeaders(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    const ProxyRetryInfoMap& proxy_retry_info,
    HttpRequestHeaders* headers) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(headers);
  OnBeforeSendHeaders(request, proxy_info, proxy_retry_info, headers);
}

int NetworkDelegate::NotifyHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    const IPEndPoint& endpoint,
    base::Optional<GURL>* preserve_fragment_on_redirect_url) {
  TRACE_EVENT0(NetTracingCategory(), "NetworkDelegate::NotifyHeadersReceived");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(original_response_headers);
  DCHECK(!callback.is_null());
  DCHECK(!preserve_fragment_on_redirect_url->has_value());
  return OnHeadersReceived(request, std::move(callback),
                           original_response_headers, override_response_headers,
                           endpoint, preserve_fragment_on_redirect_url);
}

void NetworkDelegate::NotifyResponseStarted(URLRequest* request,
                                            int net_error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(request);

  OnResponseStarted(request, net_error);
}

void NetworkDelegate::NotifyBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(request);
  OnBeforeRedirect(request, new_location);
}

void NetworkDelegate::NotifyCompleted(URLRequest* request,
                                      bool started,
                                      int net_error) {
  TRACE_EVENT0(NetTracingCategory(), "NetworkDelegate::NotifyCompleted");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(request);
  OnCompleted(request, started, net_error);
}

void NetworkDelegate::NotifyURLRequestDestroyed(URLRequest* request) {
  TRACE_EVENT0(NetTracingCategory(),
               "NetworkDelegate::NotifyURLRequestDestroyed");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(request);
  OnURLRequestDestroyed(request);
}

void NetworkDelegate::NotifyPACScriptError(int line_number,
                                           const base::string16& error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OnPACScriptError(line_number, error);
}

bool NetworkDelegate::CanGetCookies(const URLRequest& request,
                                    const CookieList& cookie_list,
                                    bool allowed_from_caller) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!(request.load_flags() & LOAD_DO_NOT_SEND_COOKIES));
  return OnCanGetCookies(request, cookie_list, allowed_from_caller);
}

bool NetworkDelegate::CanSetCookie(const URLRequest& request,
                                   const CanonicalCookie& cookie,
                                   CookieOptions* options,
                                   bool allowed_from_caller) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!(request.load_flags() & LOAD_DO_NOT_SAVE_COOKIES));
  return OnCanSetCookie(request, cookie, options, allowed_from_caller);
}

bool NetworkDelegate::ForcePrivacyMode(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) const {
  TRACE_EVENT0(NetTracingCategory(), "NetworkDelegate::ForcePrivacyMode");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return OnForcePrivacyMode(url, site_for_cookies, top_frame_origin);
}

bool NetworkDelegate::CancelURLRequestWithPolicyViolatingReferrerHeader(
    const URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      request, target_url, referrer_url);
}

bool NetworkDelegate::CanQueueReportingReport(const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return OnCanQueueReportingReport(origin);
}

void NetworkDelegate::CanSendReportingReports(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OnCanSendReportingReports(std::move(origins), std::move(result_callback));
}

bool NetworkDelegate::CanSetReportingClient(const url::Origin& origin,
                                            const GURL& endpoint) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return OnCanSetReportingClient(origin, endpoint);
}

bool NetworkDelegate::CanUseReportingClient(const url::Origin& origin,
                                            const GURL& endpoint) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return OnCanUseReportingClient(origin, endpoint);
}

}  // namespace net
