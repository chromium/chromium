// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_DELEGATE_IMPL_H_
#define NET_BASE_NETWORK_DELEGATE_IMPL_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <string>

#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_delegate.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/proxy_resolution/proxy_retry_info.h"

class GURL;

namespace url {
class Origin;
}

namespace net {

class CookieOptions;
class CookieInclusionStatus;
class HttpRequestHeaders;
class HttpResponseHeaders;
class URLRequest;

class NET_EXPORT NetworkDelegateImpl : public NetworkDelegate {
 public:
  NetworkDelegateImpl() = default;
  NetworkDelegateImpl(const NetworkDelegateImpl&) = delete;
  NetworkDelegateImpl& operator=(const NetworkDelegateImpl&) = delete;
  ~NetworkDelegateImpl() override = default;

 private:
  int OnBeforeURLRequest(URLRequest* request,
                         CompletionOnceCallback callback,
                         GURL* new_url) override;

  int OnBeforeStartTransaction(
      URLRequest* request,
      const HttpRequestHeaders& headers,
      OnBeforeStartTransactionCallback callback) override;

  int OnHeadersReceived(
      URLRequest* request,
      CompletionOnceCallback callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers,
      const IPEndPoint& endpoint,
      std::optional<GURL>* preserve_fragment_on_redirect_url) override;

  void OnBeforeRedirect(URLRequest* request, const GURL& new_location) override;

  void OnBeforeRetry(URLRequest* request) override;

  void OnResponseStarted(URLRequest* request, int net_error) override;

  void OnCompleted(URLRequest* request, bool started, int net_error) override;

  void OnURLRequestDestroyed(URLRequest* request) override;

  void OnPACScriptError(int line_number, const std::u16string& error) override;

  bool OnAnnotateAndMoveUserBlockedCookies(
      const URLRequest& request,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) override;

  bool OnCanSetCookie(
      const URLRequest& request,
      const net::CanonicalCookie& cookie,
      CookieOptions* options,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      CookieInclusionStatus* inclusion_status) override;

  std::optional<cookie_util::StorageAccessStatus> OnGetStorageAccessStatus(
      const URLRequest& request) const override;

  bool OnIsStorageAccessHeaderEnabled(const url::Origin* top_frame_origin,
                                      const GURL& url) const override;

  NetworkDelegate::PrivacySetting OnForcePrivacyMode(
      const URLRequest& request) const override;

  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;

  bool OnCanQueueReportingReport(const url::Origin& origin) const override;

  void OnCanSendReportingReports(std::set<url::Origin> origins,
                                 base::OnceCallback<void(std::set<url::Origin>)>
                                     result_callback) const override;

  bool OnCanSetReportingClient(const url::Origin& origin,
                               const GURL& endpoint) const override;

  bool OnCanUseReportingClient(const url::Origin& origin,
                               const GURL& endpoint) const override;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_DELEGATE_IMPL_H_
