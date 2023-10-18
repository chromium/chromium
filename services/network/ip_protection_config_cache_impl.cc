// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection_config_cache_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "services/network/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection_proxy_list_manager_impl.h"
#include "services/network/ip_protection_token_cache_manager.h"
#include "services/network/ip_protection_token_cache_manager_impl.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

IpProtectionConfigCacheImpl::IpProtectionConfigCacheImpl(
    mojo::PendingRemote<network::mojom::IpProtectionConfigGetter>
        config_getter) {
  // Proxy list is null upon cache creation.
  ipp_proxy_list_manager_ = nullptr;

  if (config_getter.is_valid()) {
    config_getter_.Bind(std::move(config_getter));
  }
}

IpProtectionConfigCacheImpl::~IpProtectionConfigCacheImpl() = default;

void IpProtectionConfigCacheImpl::SetUp() {
  ipp_proxy_list_manager_ =
      std::make_unique<IpProtectionProxyListManagerImpl>(&config_getter_);
  // TODO(@ciaramcmullin) Initialize kProxyB cache manager when fetching tokens
  // for proxy B is supported.
  ipp_token_cache_managers_[network::mojom::IpProtectionProxyLayer::kProxyA] =
      std::make_unique<IpProtectionTokenCacheManagerImpl>(
          &config_getter_, network::mojom::IpProtectionProxyLayer::kProxyA);
}

bool IpProtectionConfigCacheImpl::AreAuthTokensAvailable() {
  for (const auto& manager : ipp_token_cache_managers_) {
    if (!manager.second->IsAuthTokenAvailable()) {
      return false;
    }
  }
  return !ipp_token_cache_managers_.empty();
}

absl::optional<network::mojom::BlindSignedAuthTokenPtr>
IpProtectionConfigCacheImpl::GetAuthToken(
    network::mojom::IpProtectionProxyLayer proxy_layer) {
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> result;
  if (ipp_token_cache_managers_.count(proxy_layer) > 0) {
    result = ipp_token_cache_managers_[proxy_layer]->GetAuthToken();
  }
  return result;
}

void IpProtectionConfigCacheImpl::InvalidateTryAgainAfterTime() {
  for (const auto& manager : ipp_token_cache_managers_) {
    manager.second->InvalidateTryAgainAfterTime();
  }
}

void IpProtectionConfigCacheImpl::SetIpProtectionTokenCacheManagerForTesting(
    network::mojom::IpProtectionProxyLayer proxy_layer,
    std::unique_ptr<IpProtectionTokenCacheManager> ipp_token_cache_manager) {
  ipp_token_cache_managers_[proxy_layer] = std::move(ipp_token_cache_manager);
}

IpProtectionTokenCacheManager*
IpProtectionConfigCacheImpl::GetIpProtectionTokenCacheManagerForTesting(
    network::mojom::IpProtectionProxyLayer proxy_layer) {
  return ipp_token_cache_managers_[proxy_layer].get();
}

void IpProtectionConfigCacheImpl::SetIpProtectionProxyListManagerForTesting(
    std::unique_ptr<IpProtectionProxyListManager> ipp_proxy_list_manager) {
  ipp_proxy_list_manager_ = std::move(ipp_proxy_list_manager);
}

bool IpProtectionConfigCacheImpl::IsProxyListAvailable() {
  return (ipp_proxy_list_manager_ != nullptr)
             ? ipp_proxy_list_manager_->IsProxyListAvailable()
             : false;
}

const std::vector<std::string>& IpProtectionConfigCacheImpl::GetProxyList() {
  static const std::vector<std::string> empty_vector;
  return (ipp_proxy_list_manager_ != nullptr)
             ? ipp_proxy_list_manager_->ProxyList()
             : empty_vector;
}

void IpProtectionConfigCacheImpl::RequestRefreshProxyList() {
  if (ipp_proxy_list_manager_ != nullptr) {
    ipp_proxy_list_manager_->RequestRefreshProxyList();
  }
}

}  // namespace network
