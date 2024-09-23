// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate.h"

#include <utility>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/threading/thread_checker.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
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
    const HttpRequestHeaders& headers,
    OnBeforeStartTransactionCallback callback) {
  TRACE_EVENT0(NetTracingCategory(),
               "NetworkDelegate::NotifyBeforeStartTransation");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!callback.is_null());
  return OnBeforeStartTransaction(request, headers, std::move(callback));
}

int NetworkDelegate::NotifyHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    const IPEndPoint& endpoint,
    std::optional<GURL>* preserve_fragment_on_redirect_url) {
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

void NetworkDelegate::NotifyBeforeRetry(URLRequest* request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(request);
  OnBeforeRetry(request);
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
                                           const std::u16string& error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OnPACScriptError(line_number, error);
}

bool NetworkDelegate::AnnotateAndMoveUserBlockedCookies(
    const URLRequest& request,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    net::CookieAccessResultList& maybe_included_cookies,
    net::CookieAccessResultList& excluded_cookies) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool allowed = OnAnnotateAndMoveUserBlockedCookies(
      request, first_party_set_metadata, maybe_included_cookies,
      excluded_cookies);
  cookie_util::DCheckIncludedAndExcludedCookieLists(maybe_included_cookies,
                                                    excluded_cookies);
  return allowed;
}

bool NetworkDelegate::CanSetCookie(
    const URLRequest& request,
    const CanonicalCookie& cookie,
    CookieOptions* options,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    CookieInclusionStatus* inclusion_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!(request.load_flags() & LOAD_DO_NOT_SAVE_COOKIES));
  return OnCanSetCookie(request, cookie, options, first_party_set_metadata,
                        inclusion_status);
}

std::optional<cookie_util::StorageAccessStatus>
NetworkDelegate::GetStorageAccessStatus(const URLRequest& request) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return OnGetStorageAccessStatus(request);
}

bool NetworkDelegate::IsStorageAccessHeaderEnabled(
    const url::Origin* top_frame_origin,
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return OnIsStorageAccessHeaderEnabled(top_frame_origin, url);
}

NetworkDelegate::PrivacySetting NetworkDelegate::ForcePrivacyMode(
    const URLRequest& request) const {
  TRACE_EVENT0(NetTracingCategory(), "NetworkDelegate::ForcePrivacyMode");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return OnForcePrivacyMode(request);
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

// static
void NetworkDelegate::ExcludeAllCookies(
    net::CookieInclusionStatus::ExclusionReason reason,
    net::CookieAccessResultList& maybe_included_cookies,
    net::CookieAccessResultList& excluded_cookies) {
  excluded_cookies.insert(
      excluded_cookies.end(),
      std::make_move_iterator(maybe_included_cookies.begin()),
      std::make_move_iterator(maybe_included_cookies.end()));
  maybe_included_cookies.clear();
  // Add the ExclusionReason for all cookies.
  for (net::CookieWithAccessResult& cookie : excluded_cookies) {
    cookie.access_result.status.AddExclusionReason(reason);
  }
}

// static
void NetworkDelegate::ExcludeAllCookiesExceptPartitioned(
    net::CookieInclusionStatus::ExclusionReason reason,
    net::CookieAccessResultList& maybe_included_cookies,
    net::CookieAccessResultList& excluded_cookies) {
  // If cookies are not universally disabled, we will preserve partitioned
  // cookies
  const auto to_be_moved = base::ranges::stable_partition(
      maybe_included_cookies, [](const net::CookieWithAccessResult& cookie) {
        return cookie.cookie.IsPartitioned();
      });
  excluded_cookies.insert(
      excluded_cookies.end(), std::make_move_iterator(to_be_moved),
      std::make_move_iterator(maybe_included_cookies.end()));
  maybe_included_cookies.erase(to_be_moved, maybe_included_cookies.end());

  // Add the ExclusionReason for all excluded cookies.
  for (net::CookieWithAccessResult& cookie : excluded_cookies) {
    cookie.access_result.status.AddExclusionReason(reason);
  }
}

// static
void NetworkDelegate::MoveExcludedCookies(
    net::CookieAccessResultList& maybe_included_cookies,
    net::CookieAccessResultList& excluded_cookies) {
  const auto to_be_moved = base::ranges::stable_partition(
      maybe_included_cookies, [](const CookieWithAccessResult& cookie) {
        return cookie.access_result.status.IsInclude();
      });
  excluded_cookies.insert(
      excluded_cookies.end(), std::make_move_iterator(to_be_moved),
      std::make_move_iterator(maybe_included_cookies.end()));
  maybe_included_cookies.erase(to_be_moved, maybe_included_cookies.end());
}
}  // namespace net
