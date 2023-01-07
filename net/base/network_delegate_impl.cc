// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate_impl.h"

#include "net/base/net_errors.h"
#include "net/first_party_sets/same_party_context.h"

namespace net {

int NetworkDelegateImpl::OnBeforeURLRequest(URLRequest* request,
                                            CompletionOnceCallback callback,
                                            GURL* new_url) {
  return OK;
}

int NetworkDelegateImpl::OnBeforeStartTransaction(
    URLRequest* request,
    const HttpRequestHeaders& headers,
    OnBeforeStartTransactionCallback callback) {
  return OK;
}

int NetworkDelegateImpl::OnHeadersReceived(
    URLRequest* request,
    CompletionOnceCallback callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    const IPEndPoint& endpoint,
    absl::optional<GURL>* preserve_fragment_on_redirect_url) {
  return OK;
}

void NetworkDelegateImpl::OnBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {}

void NetworkDelegateImpl::OnResponseStarted(URLRequest* request,
                                            int net_error) {}

void NetworkDelegateImpl::OnCompleted(URLRequest* request,
                                      bool started,
                                      int net_error) {}

void NetworkDelegateImpl::OnURLRequestDestroyed(URLRequest* request) {
}

void NetworkDelegateImpl::OnPACScriptError(int line_number,
                                           const std::u16string& error) {}

bool NetworkDelegateImpl::OnAnnotateAndMoveUserBlockedCookies(
    const URLRequest& request,
    const net::FirstPartySetMetadata& first_party_set_metadata,
    net::CookieAccessResultList& maybe_included_cookies,
    net::CookieAccessResultList& excluded_cookies) {
  return true;
}

bool NetworkDelegateImpl::OnCanSetCookie(const URLRequest& request,
                                         const net::CanonicalCookie& cookie,
                                         CookieOptions* options) {
  return true;
}

NetworkDelegate::PrivacySetting NetworkDelegateImpl::OnForcePrivacyMode(
    const GURL& url,
    const SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    SamePartyContext::Type same_party_context_type) const {
  return NetworkDelegate::PrivacySetting::kStateAllowed;
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

absl::optional<FirstPartySetsCacheFilter::MatchInfo>
NetworkDelegateImpl::OnGetFirstPartySetsCacheFilterMatchInfoMaybeAsync(
    const SchemefulSite& request_site,
    base::OnceCallback<void(FirstPartySetsCacheFilter::MatchInfo)> callback)
    const {
  return {FirstPartySetsCacheFilter::MatchInfo()};
}

}  // namespace net
