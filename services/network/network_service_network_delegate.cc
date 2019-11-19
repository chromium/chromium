// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_network_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/domain_reliability/monitor.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "services/network/cookie_manager.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/network_service_proxy_delegate.h"
#include "services/network/pending_callback_chain.h"
#include "services/network/public/cpp/features.h"
#include "services/network/url_loader.h"
#include "url/gurl.h"

#if !defined(OS_IOS)
#include "services/network/websocket.h"
#endif

namespace network {

namespace {

const char kClearSiteDataHeader[] = "Clear-Site-Data";

}  // anonymous namespace

NetworkServiceNetworkDelegate::NetworkServiceNetworkDelegate(
    bool enable_referrers,
    bool validate_referrer_policy_on_initial_request,
    mojo::PendingRemote<mojom::ProxyErrorClient> proxy_error_client_remote,
    NetworkContext* network_context)
    : enable_referrers_(enable_referrers),
      validate_referrer_policy_on_initial_request_(
          validate_referrer_policy_on_initial_request),
      network_context_(network_context) {
  if (proxy_error_client_remote)
    proxy_error_client_.Bind(std::move(proxy_error_client_remote));
}

NetworkServiceNetworkDelegate::~NetworkServiceNetworkDelegate() = default;

void NetworkServiceNetworkDelegate::MaybeTruncateReferrer(
    net::URLRequest* const request,
    const GURL& effective_url) {
  if (!enable_referrers_) {
    request->SetReferrer(std::string());
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kCapReferrerToOriginOnCrossOrigin)) {
    url::Origin destination_origin = url::Origin::Create(effective_url);
    url::Origin referrer_origin =
        url::Origin::Create(GURL(request->referrer()));
    url::Origin initiator_origin =
        request->initiator().value_or(referrer_origin);
    bool same_origin = initiator_origin.IsSameOriginWith(destination_origin);
    if (!same_origin)
      request->SetReferrer(referrer_origin.GetURL().spec());
  }
}

int NetworkServiceNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    GURL* new_url) {
  DCHECK(request);

  auto* const loader = URLLoader::ForRequest(*request);
  const GURL* effective_url = nullptr;
  if (loader && loader->new_redirect_url()) {
    DCHECK(new_url);
    *new_url = loader->new_redirect_url().value();
    effective_url = new_url;
  } else {
    effective_url = &request->url();
  }

  MaybeTruncateReferrer(request, *effective_url);

  NetworkService* network_service = network_context_->network_service();
  if (network_service)
    network_service->OnBeforeURLRequest();

  if (!loader)
    return net::OK;

  if (network_service) {
    loader->SetAllowReportingRawHeaders(network_service->HasRawHeadersAccess(
        loader->GetProcessId(), *effective_url));
  }
  return net::OK;
}

int NetworkServiceNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* headers) {
  if (network_context_->proxy_delegate()) {
    network_context_->proxy_delegate()->OnBeforeStartTransaction(request,
                                                                 headers);
  }
  URLLoader* url_loader = URLLoader::ForRequest(*request);
  if (url_loader)
    return url_loader->OnBeforeStartTransaction(std::move(callback), headers);

#if !defined(OS_IOS)
  WebSocket* web_socket = WebSocket::ForRequest(*request);
  if (web_socket)
    return web_socket->OnBeforeStartTransaction(std::move(callback), headers);
#endif  // !defined(OS_IOS)

  return net::OK;
}

void NetworkServiceNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::HttpRequestHeaders* headers) {
  if (network_context_->proxy_delegate()) {
    network_context_->proxy_delegate()->OnBeforeSendHeaders(request, proxy_info,
                                                            headers);
  }
}

int NetworkServiceNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    const net::IPEndPoint& endpoint,
    base::Optional<GURL>* preserve_fragment_on_redirect_url) {
  auto chain = base::MakeRefCounted<PendingCallbackChain>(std::move(callback));
  URLLoader* url_loader = URLLoader::ForRequest(*request);
  if (url_loader) {
    chain->AddResult(url_loader->OnHeadersReceived(
        chain->CreateCallback(), original_response_headers,
        override_response_headers, endpoint,
        preserve_fragment_on_redirect_url));
  }

#if !defined(OS_IOS)
  WebSocket* web_socket = WebSocket::ForRequest(*request);
  if (web_socket) {
    chain->AddResult(web_socket->OnHeadersReceived(
        chain->CreateCallback(), original_response_headers,
        override_response_headers, preserve_fragment_on_redirect_url));
  }
#endif  // !defined(OS_IOS)

  chain->AddResult(HandleClearSiteDataHeader(request, chain->CreateCallback(),
                                             original_response_headers));

  return chain->GetResult();
}

void NetworkServiceNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                                     const GURL& new_location) {
  if (network_context_->domain_reliability_monitor())
    network_context_->domain_reliability_monitor()->OnBeforeRedirect(request);
}

void NetworkServiceNetworkDelegate::OnResponseStarted(net::URLRequest* request,
                                                      int net_error) {
  ForwardProxyErrors(net_error);
}

void NetworkServiceNetworkDelegate::OnCompleted(net::URLRequest* request,
                                                bool started,
                                                int net_error) {
  // TODO(mmenke): Once the network service ships on all platforms, can move
  // this logic into URLLoader's completion method.
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (network_context_->domain_reliability_monitor()) {
    network_context_->domain_reliability_monitor()->OnCompleted(request,
                                                                started);
  }

  ForwardProxyErrors(net_error);
}

void NetworkServiceNetworkDelegate::OnPACScriptError(
    int line_number,
    const base::string16& error) {
  if (!proxy_error_client_)
    return;

  proxy_error_client_->OnPACScriptError(line_number, base::UTF16ToUTF8(error));
}

bool NetworkServiceNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list,
    bool allowed_from_caller) {
  bool allowed = allowed_from_caller &&
                 network_context_->cookie_manager()
                     ->cookie_settings()
                     .IsCookieAccessAllowed(
                         request.url(), request.site_for_cookies(),
                         request.network_isolation_key().GetTopFrameOrigin());

  if (!allowed)
    return false;

  URLLoader* url_loader = URLLoader::ForRequest(request);
  if (url_loader)
    return url_loader->AllowCookies(request.url(), request.site_for_cookies());
#if !defined(OS_IOS)
  WebSocket* web_socket = WebSocket::ForRequest(request);
  if (web_socket)
    return web_socket->AllowCookies(request.url());
#endif  // !defined(OS_IOS)
  return true;
}

bool NetworkServiceNetworkDelegate::OnCanSetCookie(
    const net::URLRequest& request,
    const net::CanonicalCookie& cookie,
    net::CookieOptions* options,
    bool allowed_from_caller) {
  bool allowed = allowed_from_caller &&
                 network_context_->cookie_manager()
                     ->cookie_settings()
                     .IsCookieAccessAllowed(
                         request.url(), request.site_for_cookies(),
                         request.network_isolation_key().GetTopFrameOrigin());
  if (!allowed)
    return false;
  URLLoader* url_loader = URLLoader::ForRequest(request);
  if (url_loader)
    return url_loader->AllowCookies(request.url(), request.site_for_cookies());
#if !defined(OS_IOS)
  WebSocket* web_socket = WebSocket::ForRequest(request);
  if (web_socket)
    return web_socket->AllowCookies(request.url());
#endif  // !defined(OS_IOS)
  return true;
}

bool NetworkServiceNetworkDelegate::OnForcePrivacyMode(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) const {
  return !network_context_->cookie_manager()
              ->cookie_settings()
              .IsCookieAccessAllowed(url, site_for_cookies, top_frame_origin);
}

bool NetworkServiceNetworkDelegate::
    OnCancelURLRequestWithPolicyViolatingReferrerHeader(
        const net::URLRequest& request,
        const GURL& target_url,
        const GURL& referrer_url) const {
  // TODO(mmenke): Once the network service has shipped on all platforms,
  // consider moving this logic into URLLoader, and removing this method from
  // NetworkDelegate. Can just have a DCHECK in URLRequest instead.
  if (!validate_referrer_policy_on_initial_request_)
    return false;

  LOG(ERROR) << "Cancelling request to " << target_url
             << " with invalid referrer " << referrer_url;
  return true;
}

bool NetworkServiceNetworkDelegate::OnCanQueueReportingReport(
    const url::Origin& origin) const {
  return network_context_->cookie_manager()
      ->cookie_settings()
      .IsCookieAccessAllowed(origin.GetURL(), origin.GetURL());
}

void NetworkServiceNetworkDelegate::OnCanSendReportingReports(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  auto* client = network_context_->client();
  if (!client) {
    origins.clear();
    std::move(result_callback).Run(std::move(origins));
    return;
  }

  if (network_context_->SkipReportingPermissionCheck()) {
    std::move(result_callback).Run(std::move(origins));
    return;
  }

  std::vector<url::Origin> origin_vector;
  std::copy(origins.begin(), origins.end(), std::back_inserter(origin_vector));
  client->OnCanSendReportingReports(
      origin_vector,
      base::BindOnce(
          &NetworkServiceNetworkDelegate::FinishedCanSendReportingReports,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
}

bool NetworkServiceNetworkDelegate::OnCanSetReportingClient(
    const url::Origin& origin,
    const GURL& endpoint) const {
  return network_context_->cookie_manager()
      ->cookie_settings()
      .IsCookieAccessAllowed(origin.GetURL(), origin.GetURL());
}

bool NetworkServiceNetworkDelegate::OnCanUseReportingClient(
    const url::Origin& origin,
    const GURL& endpoint) const {
  return network_context_->cookie_manager()
      ->cookie_settings()
      .IsCookieAccessAllowed(origin.GetURL(), origin.GetURL());
}

int NetworkServiceNetworkDelegate::HandleClearSiteDataHeader(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers) {
  DCHECK(request);
  if (!original_response_headers || !network_context_->client())
    return net::OK;

  URLLoader* url_loader = URLLoader::ForRequest(*request);
  if (!url_loader)
    return net::OK;

  std::string header_value;
  if (!original_response_headers->GetNormalizedHeader(kClearSiteDataHeader,
                                                      &header_value)) {
    return net::OK;
  }

  network_context_->client()->OnClearSiteData(
      url_loader->GetProcessId(), url_loader->GetRenderFrameId(),
      request->url(), header_value, request->load_flags(),
      base::BindOnce(&NetworkServiceNetworkDelegate::FinishedClearSiteData,
                     weak_ptr_factory_.GetWeakPtr(), request->GetWeakPtr(),
                     std::move(callback)));

  return net::ERR_IO_PENDING;
}

void NetworkServiceNetworkDelegate::FinishedClearSiteData(
    base::WeakPtr<net::URLRequest> request,
    net::CompletionOnceCallback callback) {
  if (request)
    std::move(callback).Run(net::OK);
}

void NetworkServiceNetworkDelegate::FinishedCanSendReportingReports(
    base::OnceCallback<void(std::set<url::Origin>)> result_callback,
    const std::vector<url::Origin>& origins) {
  std::set<url::Origin> origin_set(origins.begin(), origins.end());
  std::move(result_callback).Run(origin_set);
}

void NetworkServiceNetworkDelegate::ForwardProxyErrors(int net_error) {
  if (!proxy_error_client_)
    return;

  // TODO(https://crbug.com/876848): Provide justification for the currently
  // enumerated errors.
  switch (net_error) {
    case net::ERR_PROXY_AUTH_UNSUPPORTED:
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
      proxy_error_client_->OnRequestMaybeFailedDueToProxySettings(net_error);
      break;
  }
}

}  // namespace network
