// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/url_loader.h"
#include "url/url_constants.h"

namespace network {
namespace {

// The maximum size of the two caches that contain the GURLs for which special
// handling is required.
constexpr size_t kMaxCacheSize = 15;

// The maximum number of previous configs to keep.
constexpr size_t kMaxPreviousConfigs = 2;

bool ApplyProxyConfigToProxyInfo(const net::ProxyConfig::ProxyRules& rules,
                                 const net::ProxyRetryInfoMap& proxy_retry_info,
                                 const GURL& url,
                                 net::ProxyInfo* proxy_info) {
  DCHECK(proxy_info);
  if (rules.empty())
    return false;

  rules.Apply(url, proxy_info);
  proxy_info->DeprioritizeBadProxies(proxy_retry_info);
  return !proxy_info->is_empty() && !proxy_info->proxy_server().is_direct();
}

// Checks if |target_proxy| is in |proxy_list|.
bool CheckProxyList(const net::ProxyList& proxy_list,
                    const net::ProxyServer& target_proxy) {
  for (const auto& proxy : proxy_list.GetAll()) {
    if (!proxy.is_direct() &&
        proxy.host_port_pair().Equals(target_proxy.host_port_pair())) {
      return true;
    }
  }
  return false;
}

// Returns true if there is a possibility that |proxy_rules->Apply()| can
// choose |target_proxy|. This does not consider the bypass rules; it only
// scans the possible set of proxy server.
bool RulesContainsProxy(const net::ProxyConfig::ProxyRules& proxy_rules,
                        const net::ProxyServer& target_proxy) {
  switch (proxy_rules.type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return false;

    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      return CheckProxyList(proxy_rules.single_proxies, target_proxy);

    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      return CheckProxyList(proxy_rules.proxies_for_http, target_proxy) ||
             CheckProxyList(proxy_rules.proxies_for_https, target_proxy);
  }

  NOTREACHED();
  return false;
}

bool IsValidCustomProxyConfig(const mojom::CustomProxyConfig& config) {
  switch (config.rules.type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return true;

    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      return !config.rules.single_proxies.IsEmpty();

    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      return !config.rules.proxies_for_http.IsEmpty() ||
             !config.rules.proxies_for_https.IsEmpty();
  }

  NOTREACHED();
  return false;
}

// Merges headers from |in| to |out|. If the header already exists in |out| they
// are combined.
void MergeRequestHeaders(net::HttpRequestHeaders* out,
                         const net::HttpRequestHeaders& in) {
  for (net::HttpRequestHeaders::Iterator it(in); it.GetNext();) {
    std::string old_value;
    if (out->GetHeader(it.name(), &old_value)) {
      out->SetHeader(it.name(), old_value + ", " + it.value());
    } else {
      out->SetHeader(it.name(), it.value());
    }
  }
}

bool IsURLBlockedForCustomProxy(const net::URLRequest& request) {
  auto* url_loader = URLLoader::ForRequest(request);
  if (url_loader && url_loader->GetProcessId() == 0 &&
      static_cast<SpecialRoutingIDs>(url_loader->GetRenderFrameId()) ==
          MSG_ROUTING_NONE) {
    // The request is not initiated by navigation or renderer. Block the request
    // from going through custom proxy. This is a temporary solution to fix the
    // bypassed downloads when using the custom proxy. See crbug.com/953166.
    // TODO(957215): Implement a better solution in download manager and remove
    // this codepath
    return true;
  }
  // If the last entry occurs earlier in the |url_chain|, then very likely there
  // is a redirect cycle.
  return std::find(request.url_chain().rbegin() + 1, request.url_chain().rend(),
                   request.url_chain().back()) != request.url_chain().rend();
}

}  // namespace

NetworkServiceProxyDelegate::NetworkServiceProxyDelegate(
    mojom::CustomProxyConfigPtr initial_config,
    mojo::PendingReceiver<mojom::CustomProxyConfigClient>
        config_client_receiver)
    : proxy_config_(std::move(initial_config)),
      receiver_(this, std::move(config_client_receiver)) {
  // Make sure there is always a valid proxy config so we don't need to null
  // check it.
  if (!proxy_config_)
    proxy_config_ = mojom::CustomProxyConfig::New();
}

void NetworkServiceProxyDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    net::HttpRequestHeaders* headers) {
  if (!MayProxyURL(request->url()))
    return;

  if (!proxy_config_->can_use_proxy_on_http_url_redirect_cycles &&
      MayHaveProxiedURL(request->url()) &&
      request->url().SchemeIs(url::kHttpScheme) &&
      IsURLBlockedForCustomProxy(*request)) {
    redirect_loop_cache_.push_front(request->url());
    if (previous_proxy_configs_.size() > kMaxCacheSize)
      redirect_loop_cache_.pop_back();
  }

  // For other schemes, the headers can be added to the CONNECT request when
  // establishing the secure tunnel instead, see OnBeforeHttp1TunnelRequest().
  const bool scheme_is_http = request->url().SchemeIs(url::kHttpScheme);
  if (scheme_is_http) {
    MergeRequestHeaders(headers, proxy_config_->pre_cache_headers);
    auto* url_loader = URLLoader::ForRequest(*request);
    if (url_loader) {
      MergeRequestHeaders(headers,
                          url_loader->custom_proxy_pre_cache_headers());
    }
  }
}

void NetworkServiceProxyDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* headers) {
  // For other schemes, the headers can be added to the CONNECT request when
  // establishing the secure tunnel instead, see OnBeforeHttp1TunnelRequest().
  if (!request->url().SchemeIs(url::kHttpScheme))
    return;

  auto* url_loader = URLLoader::ForRequest(*request);

  if (IsInProxyConfig(proxy_info.proxy_server())) {
    MergeRequestHeaders(headers, proxy_config_->post_cache_headers);

    if (url_loader) {
      MergeRequestHeaders(headers,
                          url_loader->custom_proxy_post_cache_headers());
    }
  } else if (MayHaveProxiedURL(request->url())) {
    for (const auto& kv : proxy_config_->pre_cache_headers.GetHeaderVector()) {
      headers->RemoveHeader(kv.key);
    }

    if (url_loader) {
      for (const auto& kv :
           url_loader->custom_proxy_pre_cache_headers().GetHeaderVector()) {
        headers->RemoveHeader(kv.key);
      }
    }
  }
}

NetworkServiceProxyDelegate::~NetworkServiceProxyDelegate() {}

void NetworkServiceProxyDelegate::OnResolveProxy(
    const GURL& url,
    const std::string& method,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) {
  if (!EligibleForProxy(*result, method))
    return;

  // Check if using custom proxy for |url| can result in redirect loops.
  std::deque<GURL>::const_iterator it =
      std::find(redirect_loop_cache_.begin(), redirect_loop_cache_.end(), url);
  if (it != redirect_loop_cache_.end())
    return;

  net::ProxyInfo proxy_info;
  if (ApplyProxyConfigToProxyInfo(proxy_config_->rules, proxy_retry_info, url,
                                  &proxy_info)) {
    DCHECK(!proxy_info.is_empty() && !proxy_info.is_direct());
    result->OverrideProxyList(proxy_info.proxy_list());
    GetAlternativeProxy(proxy_retry_info, result);
  }
}

void NetworkServiceProxyDelegate::OnFallback(const net::ProxyServer& bad_proxy,
                                             int net_error) {}

void NetworkServiceProxyDelegate::OnBeforeHttp1TunnelRequest(
    const net::ProxyServer& proxy_server,
    net::HttpRequestHeaders* extra_headers) {
  if (IsInProxyConfig(proxy_server))
    MergeRequestHeaders(extra_headers, proxy_config_->connect_tunnel_headers);
}

net::Error NetworkServiceProxyDelegate::OnHttp1TunnelHeadersReceived(
    const net::ProxyServer& proxy_server,
    const net::HttpResponseHeaders& response_headers) {
  return net::OK;
}

void NetworkServiceProxyDelegate::OnCustomProxyConfigUpdated(
    mojom::CustomProxyConfigPtr proxy_config) {
  DCHECK(IsValidCustomProxyConfig(*proxy_config));
  if (proxy_config_) {
    previous_proxy_configs_.push_front(std::move(proxy_config_));
    if (previous_proxy_configs_.size() > kMaxPreviousConfigs)
      previous_proxy_configs_.pop_back();
  }
  proxy_config_ = std::move(proxy_config);
}

void NetworkServiceProxyDelegate::MarkProxiesAsBad(
    base::TimeDelta bypass_duration,
    const net::ProxyList& bad_proxies_list,
    MarkProxiesAsBadCallback callback) {
  std::vector<net::ProxyServer> bad_proxies = bad_proxies_list.GetAll();

  // Synthesize a suitable |ProxyInfo| to add the proxies to the
  // |ProxyRetryInfoMap| of the proxy service.
  //
  // TODO(eroman): Support this more directly on ProxyResolutionService.
  net::ProxyList proxy_list;
  for (const auto& bad_proxy : bad_proxies)
    proxy_list.AddProxyServer(bad_proxy);
  proxy_list.AddProxyServer(net::ProxyServer::Direct());

  net::ProxyInfo proxy_info;
  proxy_info.UseProxyList(proxy_list);

  proxy_resolution_service_->MarkProxiesAsBadUntil(
      proxy_info, bypass_duration, bad_proxies, net::NetLogWithSource());

  std::move(callback).Run();
}

void NetworkServiceProxyDelegate::ClearBadProxiesCache() {
  proxy_resolution_service_->ClearBadProxiesCache();
}

bool NetworkServiceProxyDelegate::IsInProxyConfig(
    const net::ProxyServer& proxy_server) const {
  if (!proxy_server.is_valid() || proxy_server.is_direct())
    return false;

  if (RulesContainsProxy(proxy_config_->rules, proxy_server))
    return true;

  for (const auto& config : previous_proxy_configs_) {
    if (RulesContainsProxy(config->rules, proxy_server))
      return true;
  }

  return false;
}

bool NetworkServiceProxyDelegate::MayProxyURL(const GURL& url) const {
  return !proxy_config_->rules.empty();
}

bool NetworkServiceProxyDelegate::MayHaveProxiedURL(const GURL& url) const {
  if (!proxy_config_->rules.empty())
    return true;

  for (const auto& config : previous_proxy_configs_) {
    if (!config->rules.empty())
      return true;
  }

  return false;
}

bool NetworkServiceProxyDelegate::EligibleForProxy(
    const net::ProxyInfo& proxy_info,
    const std::string& method) const {
  bool has_existing_config =
      !proxy_info.is_direct() || proxy_info.proxy_list().size() > 1u;
  if (!proxy_config_->should_override_existing_config && has_existing_config)
    return false;

  if (!proxy_config_->allow_non_idempotent_methods &&
      !net::HttpUtil::IsMethodIdempotent(method)) {
    return false;
  }

  return true;
}

void NetworkServiceProxyDelegate::GetAlternativeProxy(
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) {
  net::ProxyServer resolved_proxy_server = result->proxy_server();
  DCHECK(resolved_proxy_server.is_valid());

  if (!resolved_proxy_server.is_https() ||
      !proxy_config_->assume_https_proxies_support_quic) {
    return;
  }

  net::ProxyInfo alternative_proxy_info;
  alternative_proxy_info.UseProxyServer(net::ProxyServer(
      net::ProxyServer::SCHEME_QUIC, resolved_proxy_server.host_port_pair()));
  alternative_proxy_info.DeprioritizeBadProxies(proxy_retry_info);

  if (alternative_proxy_info.is_empty())
    return;

  result->SetAlternativeProxy(alternative_proxy_info.proxy_server());
}

}  // namespace network
