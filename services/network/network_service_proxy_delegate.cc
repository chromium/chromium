// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_delegate.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "services/network/url_loader.h"
#include "url/url_constants.h"

namespace network {
namespace {

bool ApplyProxyConfigToProxyInfo(const net::ProxyConfig::ProxyRules& rules,
                                 const net::ProxyRetryInfoMap& proxy_retry_info,
                                 const GURL& url,
                                 net::ProxyInfo* proxy_info) {
  DCHECK(proxy_info);
  if (rules.empty()) {
    return false;
  }

  rules.Apply(url, proxy_info);
  proxy_info->DeprioritizeBadProxyChains(proxy_retry_info);
  return !proxy_info->is_empty() && !proxy_info->is_direct();
}

// Checks if |target_proxy| is in |proxy_list|.
bool CheckProxyList(const net::ProxyList& proxy_list,
                    const net::ProxyServer& target_proxy) {
  for (const auto& proxy_chain : proxy_list.AllChains()) {
    if (proxy_chain.is_single_proxy() &&
        proxy_chain.First().host_port_pair() == target_proxy.host_port_pair()) {
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

  NOTREACHED_IN_MIGRATION();
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

  NOTREACHED_IN_MIGRATION();
  return false;
}

// Merges headers from |in| to |out|. If the header already exists in |out| they
// are combined.
void MergeRequestHeaders(net::HttpRequestHeaders* out,
                         const net::HttpRequestHeaders& in) {
  for (net::HttpRequestHeaders::Iterator it(in); it.GetNext();) {
    std::optional<std::string> old_value = out->GetHeader(it.name());
    if (old_value) {
      out->SetHeader(it.name(), *old_value + ", " + it.value());
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

NetworkServiceProxyDelegate::~NetworkServiceProxyDelegate() = default;

void NetworkServiceProxyDelegate::OnResolveProxy(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
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
    result->OverrideProxyList(proxy_info.proxy_list());
  }
}

void NetworkServiceProxyDelegate::OnSuccessfulRequestAfterFailures(
    const net::ProxyRetryInfoMap& proxy_retry_info) {}

void NetworkServiceProxyDelegate::OnFallback(const net::ProxyChain& bad_chain,
                                             int net_error) {
  if (observer_) {
    observer_->OnFallback(bad_chain, net_error);
  }
}

net::Error NetworkServiceProxyDelegate::OnBeforeTunnelRequest(
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    net::HttpRequestHeaders* extra_headers) {
  if (IsInProxyConfig(proxy_chain)) {
    MergeRequestHeaders(extra_headers, proxy_config_->connect_tunnel_headers);
  }
  return net::OK;
}

net::Error NetworkServiceProxyDelegate::OnTunnelHeadersReceived(
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    const net::HttpResponseHeaders& response_headers) {
  if (observer_) {
    // Copy the response headers since mojo expects a ref counted object.
    observer_->OnTunnelHeadersReceived(
        proxy_chain, chain_index,
        base::MakeRefCounted<net::HttpResponseHeaders>(
            response_headers.raw_headers()));
  }
  return net::OK;
}

void NetworkServiceProxyDelegate::SetProxyResolutionService(
    net::ProxyResolutionService* proxy_resolution_service) {
  proxy_resolution_service_ = proxy_resolution_service;
}

void NetworkServiceProxyDelegate::OnCustomProxyConfigUpdated(
    mojom::CustomProxyConfigPtr proxy_config,
    OnCustomProxyConfigUpdatedCallback callback) {
  DCHECK(IsValidCustomProxyConfig(*proxy_config));
  proxy_config_ = std::move(proxy_config);
  std::move(callback).Run();
}

bool NetworkServiceProxyDelegate::IsInProxyConfig(
    const net::ProxyChain& proxy_chain) const {
  if (!proxy_chain.IsValid() || proxy_chain.is_direct()) {
    return false;
  }

  // TODO(crbug.com/40284947): Support nested proxies.
  if (proxy_chain.is_single_proxy() &&
      RulesContainsProxy(proxy_config_->rules, proxy_chain.First())) {
    return true;
  }

  return false;
}

bool NetworkServiceProxyDelegate::EligibleForProxy(
    const net::ProxyInfo& proxy_info,
    const std::string& method) const {
  bool has_existing_config =
      !proxy_info.is_direct() || proxy_info.proxy_list().size() > 1u;

  if (!proxy_config_->should_override_existing_config && has_existing_config) {
    return false;
  }

  if (!proxy_config_->allow_non_idempotent_methods &&
      !net::HttpUtil::IsMethodIdempotent(method)) {
    return false;
  }

  return true;
}

void NetworkServiceProxyDelegate::OnObserverDisconnect() {
  observer_.reset();
}

}  // namespace network
