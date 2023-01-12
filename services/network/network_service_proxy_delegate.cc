// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/url_loader.h"
#include "url/url_constants.h"

namespace network {
namespace {

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

}  // namespace

NetworkServiceProxyDelegate::NetworkServiceProxyDelegate(
    mojom::CustomProxyConfigPtr initial_config,
    mojo::PendingReceiver<mojom::CustomProxyConfigClient>
        config_client_receiver,
    mojo::PendingRemote<mojom::CustomProxyConnectionObserver> observer_remote)
    : proxy_config_(std::move(initial_config)),
      receiver_(this, std::move(config_client_receiver)) {
  // Make sure there is always a valid proxy config so we don't need to null
  // check it.
  if (!proxy_config_) {
    proxy_config_ = mojom::CustomProxyConfig::New();
  }

  // |observer_remote| is an optional param for the NetworkContext.
  if (observer_remote) {
    observer_.Bind(std::move(observer_remote));
    // Unretained is safe since |observer_| is owned by |this|.
    observer_.set_disconnect_handler(
        base::BindOnce(&NetworkServiceProxyDelegate::OnObserverDisconnect,
                       base::Unretained(this)));
  }
}

NetworkServiceProxyDelegate::~NetworkServiceProxyDelegate() {}

void NetworkServiceProxyDelegate::OnResolveProxy(
    const GURL& url,
    const std::string& method,
    const net::ProxyRetryInfoMap& proxy_retry_info,
    net::ProxyInfo* result) {
  if (!EligibleForProxy(*result, method)) {
    return;
  }

  net::ProxyInfo proxy_info;
  if (ApplyProxyConfigToProxyInfo(proxy_config_->rules, proxy_retry_info, url,
                                  &proxy_info)) {
    DCHECK(!proxy_info.is_empty() && !proxy_info.is_direct());
    if (proxy_config_->should_replace_direct &&
        !proxy_config_->should_override_existing_config) {
      MergeProxyRules(result->proxy_list(), proxy_info);
    }
    result->OverrideProxyList(proxy_info.proxy_list());
  }
}

void NetworkServiceProxyDelegate::OnFallback(const net::ProxyServer& bad_proxy,
                                             int net_error) {
  if (observer_) {
    observer_->OnFallback(bad_proxy, net_error);
  }
}

void NetworkServiceProxyDelegate::OnBeforeTunnelRequest(
    const net::ProxyServer& proxy_server,
    net::HttpRequestHeaders* extra_headers) {
  if (IsInProxyConfig(proxy_server))
    MergeRequestHeaders(extra_headers, proxy_config_->connect_tunnel_headers);
}

net::Error NetworkServiceProxyDelegate::OnTunnelHeadersReceived(
    const net::ProxyServer& proxy_server,
    const net::HttpResponseHeaders& response_headers) {
  if (observer_) {
    // Copy the response headers since mojo expects a ref counted object.
    observer_->OnTunnelHeadersReceived(
        proxy_server, base::MakeRefCounted<net::HttpResponseHeaders>(
                          response_headers.raw_headers()));
  }
  return net::OK;
}

void NetworkServiceProxyDelegate::OnCustomProxyConfigUpdated(
    mojom::CustomProxyConfigPtr proxy_config,
    OnCustomProxyConfigUpdatedCallback callback) {
  DCHECK(IsValidCustomProxyConfig(*proxy_config));
  proxy_config_ = std::move(proxy_config);
  std::move(callback).Run();
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

  return false;
}

bool NetworkServiceProxyDelegate::MayProxyURL(const GURL& url) const {
  return !proxy_config_->rules.empty();
}

bool NetworkServiceProxyDelegate::EligibleForProxy(
    const net::ProxyInfo& proxy_info,
    const std::string& method) const {
  bool has_existing_config =
      !proxy_info.is_direct() || proxy_info.proxy_list().size() > 1u;

  if (!proxy_config_->should_override_existing_config && has_existing_config &&
      !proxy_config_->should_replace_direct) {
    return false;
  }

  if (!proxy_config_->allow_non_idempotent_methods &&
      !net::HttpUtil::IsMethodIdempotent(method)) {
    return false;
  }

  return true;
}

void NetworkServiceProxyDelegate::MergeProxyRules(
    const net::ProxyList& existing_proxy_list,
    net::ProxyInfo& proxy_info) const {
  net::ProxyList custom_proxy_list = proxy_info.proxy_list();
  net::ProxyList merged_proxy_list;
  for (const auto& existing_proxy : existing_proxy_list.GetAll()) {
    if (existing_proxy.is_direct()) {
      // Replace direct option with all proxies in the custom proxy list
      for (const auto& custom_proxy : custom_proxy_list.GetAll()) {
        merged_proxy_list.AddProxyServer(custom_proxy);
      }
    } else {
      merged_proxy_list.AddProxyServer(existing_proxy);
    }
  }

  proxy_info.OverrideProxyList(merged_proxy_list);
}

void NetworkServiceProxyDelegate::OnObserverDisconnect() {
  observer_.reset();
}

}  // namespace network
