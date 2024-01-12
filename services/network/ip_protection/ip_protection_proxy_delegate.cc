// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_proxy_delegate.h"

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
#include "services/network/ip_protection/ip_protection_token_cache_manager_impl.h"
#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"
#include "services/network/url_loader.h"
#include "url/url_constants.h"

namespace network {

IpProtectionProxyDelegate::IpProtectionProxyDelegate(
    NetworkServiceProxyAllowList* network_service_proxy_allow_list,
    std::unique_ptr<IpProtectionConfigCache> ipp_config_cache)
    : network_service_proxy_allow_list_(network_service_proxy_allow_list),
      ipp_config_cache_(std::move(ipp_config_cache)) {}

IpProtectionProxyDelegate::~IpProtectionProxyDelegate() = default;

void IpProtectionProxyDelegate::SetReceiver(
    mojo::PendingReceiver<network::mojom::IpProtectionProxyDelegate>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void IpProtectionProxyDelegate::OnResolveProxy(
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
  if (!network_service_proxy_allow_list_->Matches(url,
                                                  network_anonymization_key)) {
    dvlog("proxy allow list did not match");
    return;
  }
  result->set_is_mdl_match(true);
  if (!base::FeatureList::IsEnabled(net::features::kEnableIpProtectionProxy)) {
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
      if (proxy_chain.is_single_proxy() && url.SchemeIs(url::kHttpScheme)) {
        // Proxying HTTP traffic correctly for IP Protection requires
        // multi-proxy chains to be used, so if a single-proxy chain is
        // encountered here then just fail.
        // TODO(https://crbug.com/1474932): Once chains are guaranteed to be
        // multi-proxy here, turn this into a CHECK.
        dvlog("can't proxy HTTP URL through a single-proxy chain");
        return;
      }
      proxy_list.AddProxyChain(std::move(proxy_chain));
    }
  }
  // Final fallback is to DIRECT.
  auto direct_proxy_chain = net::ProxyChain::Direct();
  if (net::features::kIpPrivacyDirectOnly.Get()) {
    // To enable measuring how much traffic would be proxied (for
    // experimentation and planning purposes), mark the direct
    // proxy chain as being for IP Protection when `kIpPrivacyDirectOnly` is
    // true. When it is false, we only care about traffic that actually went
    // through the IP Protection proxies, so don't set this flag.
    direct_proxy_chain = std::move(direct_proxy_chain).ForIpProtection();
  }
  proxy_list.AddProxyChain(std::move(direct_proxy_chain));

  if (VLOG_IS_ON(3)) {
    dvlog(base::StrCat({"setting proxy list (before deprioritization) to ",
                        proxy_list.ToDebugString()}));
  }
  result->OverrideProxyList(MergeProxyRules(result->proxy_list(), proxy_list));
  result->DeprioritizeBadProxyChains(proxy_retry_info);
  return;
}

void IpProtectionProxyDelegate::OnFallback(const net::ProxyChain& bad_chain,
                                           int net_error) {
  // If the bad proxy was an IP Protection proxy, refresh the list of IP
  // protection proxies immediately.
  if (bad_chain.is_for_ip_protection()) {
    CHECK(ipp_config_cache_);
    ipp_config_cache_->RequestRefreshProxyList();
  }
}

void IpProtectionProxyDelegate::OnBeforeTunnelRequest(
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    net::HttpRequestHeaders* extra_headers) {
  auto vlog = [](std::string message) {
    VLOG(2) << "NSPD::OnBeforeTunnelRequest() - " << message;
  };
  if (proxy_chain.is_for_ip_protection()) {
    // Temporarily support a pre-shared key for access to proxyB.
    if (chain_index == 1) {
      std::string proxy_b_psk = net::features::kIpPrivacyProxyBPsk.Get();
      if (!proxy_b_psk.empty()) {
        vlog("adding proxyB PSK");
        extra_headers->SetHeader(net::HttpRequestHeaders::kProxyAuthorization,
                                 base::StrCat({"Preshared ", proxy_b_psk}));
      }
    }
    CHECK(ipp_config_cache_);
    absl::optional<network::mojom::BlindSignedAuthTokenPtr> token =
        ipp_config_cache_->GetAuthToken(chain_index);
    if (token) {
      vlog("adding auth token");
      // The token value we have here is the full Authorization header value, so
      // we can add it verbatim.
      extra_headers->SetHeader(net::HttpRequestHeaders::kAuthorization,
                               std::move((*token)->token));
    } else {
      vlog("no token available");
    }
  } else {
    vlog("not for IP protection");
  }
}

net::Error IpProtectionProxyDelegate::OnTunnelHeadersReceived(
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    const net::HttpResponseHeaders& response_headers) {
  return net::OK;
}

void IpProtectionProxyDelegate::SetProxyResolutionService(
    net::ProxyResolutionService* proxy_resolution_service) {}

void IpProtectionProxyDelegate::VerifyIpProtectionConfigGetterForTesting(
    VerifyIpProtectionConfigGetterForTestingCallback callback) {
  CHECK(ipp_config_cache_);
  auto* ipp_token_cache_manager_impl =
      static_cast<IpProtectionTokenCacheManagerImpl*>(
          ipp_config_cache_
              ->GetIpProtectionTokenCacheManagerForTesting(  // IN-TEST
                  network::mojom::IpProtectionProxyLayer::kProxyA));
  CHECK(ipp_token_cache_manager_impl);

  // If active cache management is enabled (the default), disable it and do a
  // one-time reset of the state. Since the browser process will be driving this
  // test, this makes it easier to reason about the state of
  // `ipp_config_cache_` (for instance, if the browser process sends less
  // than the requested number of tokens, the network service won't immediately
  // request more).
  if (ipp_token_cache_manager_impl->IsCacheManagementEnabledForTesting()) {
    ipp_token_cache_manager_impl->DisableCacheManagementForTesting(  // IN-TEST
        base::BindOnce(
            [](base::WeakPtr<IpProtectionProxyDelegate> weak_ptr,
               VerifyIpProtectionConfigGetterForTestingCallback callback) {
              DCHECK(weak_ptr);
              // Drain auth tokens.
              auto& ipp_config_cache = weak_ptr->ipp_config_cache_;
              ipp_config_cache->InvalidateTryAgainAfterTime();
              while (ipp_config_cache->AreAuthTokensAvailable()) {
                ipp_config_cache->GetAuthToken(0);  // kProxyA.
              }
              // Call `PostTask()` instead of invoking the Verify method again
              // directly so that if `DisableCacheManagementForTesting()` needed
              // to wait for a `TryGetAuthTokens()` call to finish, then we
              // ensure that the stored callback has been cleared before the
              // Verify method tries to call `TryGetAuthTokens()` again.
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&IpProtectionProxyDelegate::
                                     VerifyIpProtectionConfigGetterForTesting,
                                 weak_ptr, std::move(callback)));
            },
            weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // If there is a cooldown in effect, then don't send any tokens and instead
  // send back the try again after time.
  base::Time try_auth_tokens_after =
      ipp_token_cache_manager_impl
          ->try_get_auth_tokens_after_for_testing();  // IN-TEST
  if (!try_auth_tokens_after.is_null()) {
    std::move(callback).Run(nullptr, try_auth_tokens_after);
    return;
  }

  ipp_token_cache_manager_impl
      ->SetOnTryGetAuthTokensCompletedForTesting(  // IN-TEST
          base::BindOnce(&IpProtectionProxyDelegate::
                             OnIpProtectionConfigAvailableForTesting,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
  ipp_token_cache_manager_impl->CallTryGetAuthTokensForTesting();  // IN-TEST
}

void IpProtectionProxyDelegate::OnIpProtectionConfigAvailableForTesting(
    VerifyIpProtectionConfigGetterForTestingCallback callback) {
  auto* ipp_token_cache_manager_impl =
      static_cast<IpProtectionTokenCacheManagerImpl*>(
          ipp_config_cache_
              ->GetIpProtectionTokenCacheManagerForTesting(  // IN-TEST
                  network::mojom::IpProtectionProxyLayer::kProxyA));

  absl::optional<network::mojom::BlindSignedAuthTokenPtr> result =
      ipp_config_cache_->GetAuthToken(0);  // kProxyA.
  if (result.has_value()) {
    std::move(callback).Run(std::move(result).value(), absl::nullopt);
    return;
  }
  base::Time try_auth_tokens_after =
      ipp_token_cache_manager_impl
          ->try_get_auth_tokens_after_for_testing();  // IN-TEST
  std::move(callback).Run(nullptr, try_auth_tokens_after);
}

void IpProtectionProxyDelegate::
    InvalidateIpProtectionConfigCacheTryAgainAfterTime() {
  if (!ipp_config_cache_) {
    return;
  }
  ipp_config_cache_->InvalidateTryAgainAfterTime();
}

// static
net::ProxyList IpProtectionProxyDelegate::MergeProxyRules(
    const net::ProxyList& existing_proxy_list,
    const net::ProxyList& custom_proxy_list) {
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

}  // namespace network
