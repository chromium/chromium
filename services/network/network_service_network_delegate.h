// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_NETWORK_DELEGATE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_NETWORK_DELEGATE_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_delegate_impl.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/url_request/url_request.h"
#include "services/network/cookie_settings.h"
#include "services/network/network_context.h"

namespace net {
class CookieInclusionStatus;
}  // namespace net

namespace network {

// TODO(mmenke):  Look into merging this with URLLoader, and removing the
// NetworkDelegate interface.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceNetworkDelegate
    : public net::NetworkDelegateImpl {
 public:
  // |network_context| is guaranteed to outlive this class.
  NetworkServiceNetworkDelegate(
      bool enable_referrers,
      bool validate_referrer_policy_on_initial_request,
      mojo::PendingRemote<mojom::ProxyErrorClient> proxy_error_client_remote,
      NetworkContext* network_context);

  NetworkServiceNetworkDelegate(const NetworkServiceNetworkDelegate&) = delete;
  NetworkServiceNetworkDelegate& operator=(
      const NetworkServiceNetworkDelegate&) = delete;

  ~NetworkServiceNetworkDelegate() override;

  void set_enable_referrers(bool enable_referrers) {
    enable_referrers_ = enable_referrers;
  }

 private:
  // net::NetworkDelegateImpl implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;
  int OnBeforeStartTransaction(
      net::URLRequest* request,
      const net::HttpRequestHeaders& headers,
      OnBeforeStartTransactionCallback callback) override;
  int OnHeadersReceived(
      net::URLRequest* request,
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      const net::IPEndPoint& endpoint,
      std::optional<GURL>* preserve_fragment_on_redirect_url) override;
  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnCompleted(net::URLRequest* request,
                   bool started,
                   int net_error) override;
  void OnPACScriptError(int line_number, const std::u16string& error) override;
  std::optional<net::cookie_util::StorageAccessStatus> OnGetStorageAccessStatus(
      const net::URLRequest& request) const override;
  bool OnIsStorageAccessHeaderEnabled(const url::Origin* top_frame_origin,
                                      const GURL& url) const override;
  bool OnAnnotateAndMoveUserBlockedCookies(
      const net::URLRequest& request,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) override;
  bool OnCanSetCookie(
      const net::URLRequest& request,
      const net::CanonicalCookie& cookie,
      net::CookieOptions* options,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieInclusionStatus* inclusion_status) override;
  net::NetworkDelegate::PrivacySetting OnForcePrivacyMode(
      const net::URLRequest& request) const override;
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request,
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

  int HandleClearSiteDataHeader(
      net::URLRequest* request,
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers);

  void FinishedClearSiteData(base::WeakPtr<net::URLRequest> request,
                             net::CompletionOnceCallback callback);
  void FinishedCanSendReportingReports(
      base::OnceCallback<void(std::set<url::Origin>)> result_callback,
      const std::vector<url::Origin>& origins);

  void ForwardProxyErrors(int net_error);

  // Truncates the given request's referrer if required by
  // related configuration (for instance, the enable_referrers_
  // attribute or pertinent features/flags)
  void MaybeTruncateReferrer(net::URLRequest* request,
                             const GURL& effective_url);

  bool enable_referrers_;
  bool validate_referrer_policy_on_initial_request_;
  mojo::Remote<mojom::ProxyErrorClient> proxy_error_client_;
  raw_ptr<NetworkContext> network_context_;

  mutable base::WeakPtrFactory<NetworkServiceNetworkDelegate> weak_ptr_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_NETWORK_DELEGATE_H_
