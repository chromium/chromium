// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_network_delegate.h"

#include "services/network/cookie_manager.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/network_service_proxy_delegate.h"
#include "services/network/public/cpp/features.h"
#include "services/network/url_loader.h"

namespace network {

namespace {

const char kClearSiteDataHeader[] = "Clear-Site-Data";

}  // anonymous namespace

NetworkServiceNetworkDelegate::NetworkServiceNetworkDelegate(
    NetworkContext* network_context)
    : network_context_(network_context), weak_ptr_factory_(this) {}

NetworkServiceNetworkDelegate::~NetworkServiceNetworkDelegate() = default;

int NetworkServiceNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* headers) {
  if (network_context_->proxy_delegate()) {
    network_context_->proxy_delegate()->OnBeforeStartTransaction(request,
                                                                 headers);
  }
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
    GURL* allowed_unsafe_redirect_url) {
  // Clear-Site-Data header will be handled by |ResourceDispatcherHost|.
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return net::OK;

  return HandleClearSiteDataHeader(request, std::move(callback),
                                   original_response_headers);
}

bool NetworkServiceNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list,
    bool allowed_from_caller) {
  URLLoader* url_loader = URLLoader::ForRequest(request);
  if (url_loader && network_context_->network_service()->client()) {
    network_context_->network_service()->client()->OnCookiesRead(
        url_loader->GetProcessId(), url_loader->GetRenderFrameId(),
        request.url(), request.site_for_cookies(), cookie_list,
        !allowed_from_caller);
  }
  return allowed_from_caller;
}

bool NetworkServiceNetworkDelegate::OnCanSetCookie(
    const net::URLRequest& request,
    const net::CanonicalCookie& cookie,
    net::CookieOptions* options,
    bool allowed_from_caller) {
  URLLoader* url_loader = URLLoader::ForRequest(request);
  if (url_loader && network_context_->network_service()->client()) {
    network_context_->network_service()->client()->OnCookieChange(
        url_loader->GetProcessId(), url_loader->GetRenderFrameId(),
        request.url(), request.site_for_cookies(), cookie,
        !allowed_from_caller);
  }
  return allowed_from_caller;
}

bool NetworkServiceNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  // Match the default implementation (BasicNetworkDelegate)'s behavior for
  // now.
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
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(request);
  if (!original_response_headers ||
      !network_context_->network_service()->client())
    return net::OK;

  URLLoader* url_loader = URLLoader::ForRequest(*request);
  if (!url_loader)
    return net::OK;

  std::string header_value;
  if (!original_response_headers->GetNormalizedHeader(kClearSiteDataHeader,
                                                      &header_value))
    return net::OK;

  network_context_->network_service()->client()->OnClearSiteData(
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

}  // namespace network
