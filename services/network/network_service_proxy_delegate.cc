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
#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"
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
        proxy_chain.GetProxyServer(/*chain_index=*/0).host_port_pair() ==
            target_proxy.host_port_pair()) {
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
    mojo::PendingRemote<mojom::CustomProxyConnectionObserver> observer_remote,
    NetworkServiceProxyAllowList* network_service_proxy_allow_list)
    : proxy_config_(std::move(initial_config)),
      receiver_(this, std::move(config_client_receiver)),
      network_service_proxy_allow_list_(network_service_proxy_allow_list) {
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
  auto dvlog = [&](std::string message) {
    absl::optional<net::SchemefulSite> top_frame_site =
        network_anonymization_key.GetTopFrameSite();
    DVLOG(3) << "NSPD::OnResolveProxy(" << url << ", "
             << (top_frame_site.has_value() ? top_frame_site.value()
                                            : net::SchemefulSite())
             << ") - " << message;
  };
  if (IsForIpProtection()) {
    // Note: We do not proxy requests if:
    // - The allow list is not available or is not enabled.
    // - The request doesn't match the allow list.
    // - The token cache is not available.
    // - The token cache does not have tokens.
    // - No proxy list is available.
    // - `kEnableIpProtection` is `false`.
    // - `kIpPrivacyDirectOnly` is `true`.
    if (!network_service_proxy_allow_list_) {
      dvlog("no proxy allow list");
      return;
    }
    if (!network_service_proxy_allow_list_->IsEnabled()) {
      dvlog("proxy allow list not enabled");
      return;
    }
    if (!network_service_proxy_allow_list_->Matches(
            url, network_anonymization_key)) {
      dvlog("proxy allow list did not match");
      return;
    }
    result->set_is_mdl_match(true);
    if (!base::FeatureList::IsEnabled(
            net::features::kEnableIpProtectionProxy)) {
      dvlog("ip protection proxy not enabled");
      return;
    }
    if (!ipp_config_cache_) {
      dvlog("no cache");
      return;
    }
    if (!ipp_config_cache_->AreAuthTokensAvailable()) {
      dvlog("no auth token available from cache");
      return;
    }
    if (!ipp_config_cache_->IsProxyListAvailable()) {
      // NOTE: When this `vlog()` is removed, there's no need to distinguish
      // the case where a proxy list has not been downloaded, and the case
      // where a proxy list is empty. The `IsProxyListAvailable()` method can
      // be removed at that time.
      dvlog("no proxy list available from cache");
      return;
    }

    net::ProxyList proxy_list;
    if (!net::features::kIpPrivacyDirectOnly.Get()) {
      const std::vector<net::ProxyChain>& proxy_chain_list =
          ipp_config_cache_->GetProxyChainList();
      for (const auto& proxy_chain : proxy_chain_list) {
        proxy_list.AddProxyChain(std::move(proxy_chain));
      }
    }
    // Final fallback is to DIRECT.
    proxy_list.AddProxyChain(net::ProxyChain::Direct());

    if (VLOG_IS_ON(3)) {
      dvlog(base::StrCat({"setting proxy list (before deprioritization) to ",
                          proxy_list.ToDebugString()}));
    }
    result->set_is_for_ip_protection(true);
    result->OverrideProxyList(
        MergeProxyRules(result->proxy_list(), proxy_list));
    result->DeprioritizeBadProxyChains(proxy_retry_info);
    return;
  }

  // At this point, this delegate is not supporting IP protection, so apply the
  // `proxy_config_` as usual.
  if (!EligibleForProxy(*result, method)) {
    return;
  }

  net::ProxyInfo proxy_info;
  if (ApplyProxyConfigToProxyInfo(proxy_config_->rules, proxy_retry_info, url,
                                  &proxy_info)) {
    DCHECK(!proxy_info.is_empty() && !proxy_info.is_direct());
    if (proxy_config_->should_replace_direct &&
        !proxy_config_->should_override_existing_config) {
      result->OverrideProxyList(
          MergeProxyRules(result->proxy_list(), proxy_info.proxy_list()));
    } else {
      result->OverrideProxyList(proxy_info.proxy_list());
    }
  }
}

void NetworkServiceProxyDelegate::OnFallback(const net::ProxyChain& bad_chain,
                                             int net_error) {
  // If the bad proxy was an IP Protection proxy, refresh the list of IP
  // protection proxies immediately.
  if (IsProxyForIpProtection(bad_chain) && ipp_config_cache_) {
    ipp_config_cache_->RequestRefreshProxyList();
  }

  if (observer_) {
    observer_->OnFallback(bad_chain, net_error);
  }
}

void NetworkServiceProxyDelegate::OnBeforeTunnelRequest(
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    net::HttpRequestHeaders* extra_headers) {

  auto vlog = [](std::string message) {
    VLOG(2) << "NSPD::OnBeforeTunnelRequest() - " << message;
  };
  if (IsInProxyConfig(proxy_chain)) {
    MergeRequestHeaders(extra_headers, proxy_config_->connect_tunnel_headers);
  }
  if (IsForIpProtection() && IsProxyForIpProtection(proxy_chain)) {
    if (ipp_config_cache_) {
      // Temporarily support a pre-shared key for access to proxyB.
      if (chain_index == 1) {
        std::string proxy_b_psk = net::features::kIpPrivacyProxyBPsk.Get();
        if (!proxy_b_psk.empty()) {
          vlog("adding proxyB PSK");
          extra_headers->SetHeader(net::HttpRequestHeaders::kProxyAuthorization,
                                   base::StrCat({"Preshared ", proxy_b_psk}));
        }
      }
      absl::optional<network::mojom::BlindSignedAuthTokenPtr> token =
          ipp_config_cache_->GetAuthToken(chain_index);
      if (token) {
        vlog("adding auth token");
        // The token value we have here is the full Authorization header value,
        // so we can add it verbatim.
        extra_headers->SetHeader(net::HttpRequestHeaders::kAuthorization,
                                 std::move((*token)->token));
      } else {
        vlog("no token available");
      }
    } else {
      vlog("no auth token cache");
    }
  } else {
    vlog("not for IP protection");
  }
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
  std::vector<net::ProxyChain> bad_proxies = bad_proxies_list.AllChains();

  // Synthesize a suitable |ProxyInfo| to add the proxies to the
  // |ProxyRetryInfoMap| of the proxy service.
  //
  // TODO(eroman): Support this more directly on ProxyResolutionService.
  net::ProxyList proxy_list;
  for (const auto& bad_proxy : bad_proxies) {
    proxy_list.AddProxyChain(bad_proxy);
  }
  proxy_list.AddProxyChain(net::ProxyChain::Direct());

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
    const net::ProxyChain& proxy_chain) const {
  if (!proxy_chain.IsValid() || proxy_chain.is_direct()) {
    return false;
  }

  // TODO(https://crbug.com/1491092): Support nested proxies.
  if (proxy_chain.is_single_proxy() &&
      RulesContainsProxy(proxy_config_->rules,
                         proxy_chain.GetProxyServer(/*chain_index=*/0))) {
    return true;
  }

  return false;
}

bool NetworkServiceProxyDelegate::MayProxyURL(const GURL& url) const {
  return !proxy_config_->rules.empty();
}

bool NetworkServiceProxyDelegate::IsForIpProtection() {
  // Only IP protection uses the network service proxy allow list, so this
  // config represents IP protection if and only if the allow list is in use.
  return proxy_config_->rules.restrict_to_network_service_proxy_allow_list;
}

bool NetworkServiceProxyDelegate::IsProxyForIpProtection(
    const net::ProxyChain& proxy_chain) const {
  if (!ipp_config_cache_) {
    return false;
  }
  return base::Contains(ipp_config_cache_->GetProxyChainList(), proxy_chain);
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

net::ProxyList NetworkServiceProxyDelegate::MergeProxyRules(
    const net::ProxyList& existing_proxy_list,
    const net::ProxyList& custom_proxy_list) const {
  net::ProxyList merged_proxy_list;
  for (const auto& existing_chain : existing_proxy_list.AllChains()) {
    if (existing_chain.is_direct()) {
      // Replace direct option with all proxies in the custom proxy list
      for (const auto& custom_chain : custom_proxy_list.AllChains()) {
        merged_proxy_list.AddProxyChain(custom_chain);
      }
    } else {
      merged_proxy_list.AddProxyChain(existing_chain);
    }
  }

  return merged_proxy_list;
}

void NetworkServiceProxyDelegate::OnObserverDisconnect() {
  observer_.reset();
}

}  // namespace network
