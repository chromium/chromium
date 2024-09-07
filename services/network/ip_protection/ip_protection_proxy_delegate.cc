// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_proxy_delegate.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_manager_impl.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
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

using ip_protection::MaskedDomainListManager;

}  // namespace

IpProtectionProxyDelegate::IpProtectionProxyDelegate(
    MaskedDomainListManager* masked_domain_list_manager,
    std::unique_ptr<ip_protection::IpProtectionConfigCache> ipp_config_cache,
    bool is_ip_protection_enabled)
    : masked_domain_list_manager_(masked_domain_list_manager),
      ipp_config_cache_(std::move(ipp_config_cache)),
      is_ip_protection_enabled_(is_ip_protection_enabled) {
  CHECK(masked_domain_list_manager_);
  CHECK(masked_domain_list_manager_->IsEnabled());
  CHECK(ipp_config_cache_);
}

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
    std::optional<net::SchemefulSite> top_frame_site =
        network_anonymization_key.GetTopFrameSite();
    DVLOG(3) << "IPPD::OnResolveProxy(" << url << ", "
             << (top_frame_site.has_value() ? top_frame_site.value()
                                            : net::SchemefulSite())
             << ") - " << message;
  };

  const std::string& always_proxy = net::features::kIpPrivacyAlwaysProxy.Get();
  if (!always_proxy.empty()) {
    if (url.host() != always_proxy) {
      return;
    }
  } else {
    // Note: We do not proxy requests if:
    // - The allow list has not been populated.
    // - The request doesn't match the allow list.
    // - The token cache is not available.
    // - The token cache does not have tokens.
    // - No proxy list is available.
    // - `kEnableIpProtection` is `false`.
    // - `is_ip_protection_enabled_` is `false` (in other words, the user has
    //   disabled IP Protection via user settings).
    // - `kIpPrivacyDirectOnly` is `true`.
    if (!CheckEligibility(url, network_anonymization_key)) {
      return;
    }
    result->set_is_mdl_match(true);

    // TODO(https://crbug.com/40947771): Once the WebView traffic experiment is
    // done and IpProtectionProxyDelegate is only created in cases where IP
    // Protection should be used, remove this check.
    if (!base::FeatureList::IsEnabled(
            net::features::kEnableIpProtectionProxy)) {
      dvlog("ip protection proxy cannot be enabled");
      return;
    }

    if (!is_ip_protection_enabled_) {
      dvlog("ip protection proxy is not currently enabled");
      return;
    }
    const bool available = CheckAvailability(url, network_anonymization_key);
    if (!available) {
      return;
    }
  }

  net::ProxyList proxy_list;
  if (!net::features::kIpPrivacyDirectOnly.Get()) {
    const std::vector<net::ProxyChain>& proxy_chain_list =
        ipp_config_cache_->GetProxyChainList();
    for (const auto& proxy_chain : proxy_chain_list) {
      // Proxying HTTP traffic over HTTPS/SPDY proxies requires multi-proxy
      // chains.
      CHECK(proxy_chain.is_multi_proxy());

      // For debugging..
      if (net::features::kIpPrivacyUseSingleProxy.Get()) {
        proxy_list.AddProxyChain(net::ProxyChain::ForIpProtection({
            proxy_chain.GetProxyServer(0),
        }));
      } else {
        proxy_list.AddProxyChain(std::move(proxy_chain));
      }
    }
  }

  if (net::features::kIpPrivacyFallbackToDirect.Get()) {
    // Final fallback is to DIRECT.
    auto direct_proxy_chain = net::ProxyChain::Direct();
    if (net::features::kIpPrivacyDirectOnly.Get()) {
      // To enable measuring how much traffic would be proxied (for
      // experimentation and planning purposes), mark the direct
      // proxy chain as being for IP Protection when `kIpPrivacyDirectOnly` is
      // true. When it is false, we only care about traffic that actually went
      // through the IP Protection proxies, so don't set this flag.
      direct_proxy_chain = net::ProxyChain::ForIpProtection({});
    }
    proxy_list.AddProxyChain(std::move(direct_proxy_chain));
  }

  if (VLOG_IS_ON(3)) {
    dvlog(base::StrCat({"setting proxy list (before deprioritization) to ",
                        proxy_list.ToDebugString()}));
  }
  result->OverrideProxyList(MergeProxyRules(result->proxy_list(), proxy_list));
  result->DeprioritizeBadProxyChains(proxy_retry_info);
  return;
}

bool IpProtectionProxyDelegate::CheckEligibility(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key) const {
  ip_protection::ProtectionEligibility eligibility;
  bool eligible;

  auto dvlog = [&](std::string message) {
    std::optional<net::SchemefulSite> top_frame_site =
        network_anonymization_key.GetTopFrameSite();
    DVLOG(3) << "IPPD::CheckEligibility(" << url << ", "
             << (top_frame_site.has_value() ? top_frame_site.value()
                                            : net::SchemefulSite())
             << ") - " << message;
  };
  if (!masked_domain_list_manager_->IsPopulated()) {
    dvlog("proxy allow list not populated");
    eligibility = ip_protection::ProtectionEligibility::kUnknown;
    eligible = false;
  } else if (!masked_domain_list_manager_->Matches(url,
                                                   network_anonymization_key)) {
    dvlog("proxy allow list did not match");
    eligibility = ip_protection::ProtectionEligibility::kIneligible;
    eligible = false;
  } else {
    dvlog("proxy allow list matched");
    eligibility = ip_protection::ProtectionEligibility::kEligible;
    eligible = true;
  }

  ip_protection::Telemetry().RequestIsEligibleForProtection(eligibility);
  return eligible;
}

bool IpProtectionProxyDelegate::CheckAvailability(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key) const {
  auto dvlog = [&](std::string message) {
    std::optional<net::SchemefulSite> top_frame_site =
        network_anonymization_key.GetTopFrameSite();
    DVLOG(3) << "IPPD::CheckAvailability(" << url << ", "
             << (top_frame_site.has_value() ? top_frame_site.value()
                                            : net::SchemefulSite())
             << ") - " << message;
  };
  const bool auth_tokens_are_available =
      ipp_config_cache_->AreAuthTokensAvailable();
  const bool proxy_list_is_available =
      ipp_config_cache_->IsProxyListAvailable();
  ip_protection::Telemetry().ProtectionIsAvailableForRequest(
      auth_tokens_are_available, proxy_list_is_available);
  if (!auth_tokens_are_available) {
    dvlog("no auth token available from cache");
    return false;
  }
  if (!proxy_list_is_available) {
    // NOTE: When this `vlog()` and histogram are removed, there's no need to
    // distinguish the case where a proxy list has not been downloaded, and the
    // case where a proxy list is empty. The `IsProxyListAvailable()` method can
    // be removed at that time.
    dvlog("no proxy list available from cache");
    return false;
  }
  return true;
}

void IpProtectionProxyDelegate::OnSuccessfulRequestAfterFailures(
    const net::ProxyRetryInfoMap& proxy_retry_info) {
  if (!ipp_config_cache_) {
    return;
  }

  // A request was successful, but one or more proxies failed. If _only_ QUIC
  // proxies failed, then we assume this is because QUIC is not working on this
  // network, and stop injecting QUIC proxies into the proxy list.
  bool seen_quic = false;
  for (const auto& chain_and_info : proxy_retry_info) {
    const net::ProxyChain& proxy_chain = chain_and_info.first;
    if (!proxy_chain.is_for_ip_protection()) {
      continue;
    }
    const net::ProxyServer& proxy_server = proxy_chain.First();
    if (proxy_server.is_quic()) {
      seen_quic = true;
    } else {
      // A non-QUIC chain has failed.
      return;
    }
  }

  if (seen_quic) {
    // Only QUIC chains failed.
    ipp_config_cache_->QuicProxiesFailed();
  }
}

void IpProtectionProxyDelegate::OnFallback(const net::ProxyChain& bad_chain,
                                           int net_error) {
  // If the bad proxy was an IP Protection proxy, refresh the list of IP
  // protection proxies immediately.
  if (bad_chain.is_for_ip_protection()) {
    ip_protection::Telemetry().ProxyChainFallback(
        bad_chain.ip_protection_chain_id());
    ipp_config_cache_->RequestRefreshProxyList();
  }
}

net::Error IpProtectionProxyDelegate::OnBeforeTunnelRequest(
    const net::ProxyChain& proxy_chain,
    size_t chain_index,
    net::HttpRequestHeaders* extra_headers) {
  auto vlog = [](std::string message) {
    VLOG(2) << "NSPD::OnBeforeTunnelRequest() - " << message;
  };
  if (proxy_chain.is_for_ip_protection()) {
    std::optional<ip_protection::BlindSignedAuthToken> token =
        ipp_config_cache_->GetAuthToken(chain_index);
    if (token) {
      vlog("adding auth token");
      // The token value we have here is the full Authorization header value, so
      // we can add it verbatim.
      extra_headers->SetHeader(net::HttpRequestHeaders::kAuthorization,
                               std::move(token->token));
    } else {
      vlog("no token available");
      // This is an unexpected circumstance, but does happen in the wild. Rather
      // than send the request to the proxy, which will reply with an error,
      // mark the connection as failed immediately.
      return net::ERR_TUNNEL_CONNECTION_FAILED;
    }
  } else {
    vlog("not for IP protection");
  }
  int experiment_arm = net::features::kIpPrivacyDebugExperimentArm.Get();
  if (experiment_arm != 0) {
    extra_headers->SetHeader("Ip-Protection-Debug-Experiment-Arm",
                             base::NumberToString(experiment_arm));
  }
  return net::OK;
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
  auto* ipp_token_manager_impl =
      static_cast<ip_protection::IpProtectionTokenManagerImpl*>(
          ipp_config_cache_->GetIpProtectionTokenManagerForTesting(  // IN-TEST
              ip_protection::ProxyLayer::kProxyA));
  CHECK(ipp_token_manager_impl);

  // If active cache management is enabled (the default), disable it and do a
  // one-time reset of the state. Since the browser process will be driving this
  // test, this makes it easier to reason about the state of
  // `ipp_config_cache_` (for instance, if the browser process sends less
  // than the requested number of tokens, the network service won't immediately
  // request more).
  if (ipp_token_manager_impl->IsCacheManagementEnabledForTesting()) {
    ipp_token_manager_impl->DisableCacheManagementForTesting(  // IN-TEST
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
      ipp_token_manager_impl
          ->try_get_auth_tokens_after_for_testing();  // IN-TEST
  if (!try_auth_tokens_after.is_null()) {
    std::move(callback).Run(std::nullopt, try_auth_tokens_after);
    return;
  }

  ipp_token_manager_impl->SetOnTryGetAuthTokensCompletedForTesting(  // IN-TEST
      base::BindOnce(
          &IpProtectionProxyDelegate::OnIpProtectionConfigAvailableForTesting,
          weak_factory_.GetWeakPtr(), std::move(callback)));
  ipp_token_manager_impl->CallTryGetAuthTokensForTesting();  // IN-TEST
}

void IpProtectionProxyDelegate::SetIpProtectionEnabled(bool enabled) {
  is_ip_protection_enabled_ = enabled;
  // TODO(crbug.com/41494110): Tear down all existing proxied
  // HTTP/SPDY/QUIC sessions if the settings goes from being enabled to being
  // disabled. For HTTP and SPDY we could just simulate an IP address change and
  // tear down all connections rather easily, but for QUIC it's more complicated
  // because with network change session migration the connections might still
  // persist. More investigation is needed here.
  // TODO(crbug.com/41494110): Propagate this change to the config cache,
  // proxy list manager, and token cache manager to cancel further requests or
  // reschedule them. Note that as currently implemented, the token cache
  // manager will already stop requesting tokens soon after IP Protection is
  // disabled via the try again after time returned by the next TryGetAuthToken
  // call, but the GetProxyConfig calls will continue and receive failures until
  // the feature is re-enabled.
}

void IpProtectionProxyDelegate::IsIpProtectionEnabledForTesting(
    IsIpProtectionEnabledForTestingCallback callback) {
  std::move(callback).Run(is_ip_protection_enabled_);
}

void IpProtectionProxyDelegate::OnIpProtectionConfigAvailableForTesting(
    VerifyIpProtectionConfigGetterForTestingCallback callback) {
  auto* ipp_token_manager_impl =
      static_cast<ip_protection::IpProtectionTokenManagerImpl*>(
          ipp_config_cache_->GetIpProtectionTokenManagerForTesting(  // IN-TEST
              ip_protection::ProxyLayer::kProxyA));
  auto* ipp_proxy_config_manager_impl =
      static_cast<ip_protection::IpProtectionProxyConfigManagerImpl*>(
          ipp_config_cache_
              ->GetIpProtectionProxyConfigManagerForTesting());  // IN-TEST
  CHECK(ipp_proxy_config_manager_impl);
  ipp_proxy_config_manager_impl->SetProxyListForTesting(  // IN-TEST
      std::vector{net::ProxyChain::ForIpProtection(
          std::vector{net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "proxy-a", std::nullopt)})},
      ip_protection::GetGeoHintFromGeoIdForTesting(
          ipp_token_manager_impl->CurrentGeo()));
  std::optional<ip_protection::BlindSignedAuthToken> result =
      ipp_config_cache_->GetAuthToken(0);  // kProxyA.
  if (result.has_value()) {
    std::move(callback).Run(std::move(result.value()), std::nullopt);
    return;
  }
  base::Time try_auth_tokens_after =
      ipp_token_manager_impl
          ->try_get_auth_tokens_after_for_testing();  // IN-TEST
  std::move(callback).Run(std::nullopt, try_auth_tokens_after);
}

void IpProtectionProxyDelegate::
    InvalidateIpProtectionConfigCacheTryAgainAfterTime() {
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
