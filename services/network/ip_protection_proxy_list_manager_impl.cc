// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection_proxy_list_manager_impl.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network {

IpProtectionProxyListManagerImpl::IpProtectionProxyListManagerImpl(
    mojo::Remote<network::mojom::IpProtectionConfigGetter>* config_getter,
    bool disable_proxy_refreshing_for_testing)
    : config_getter_(config_getter),
      disable_proxy_refreshing_for_testing_(
          disable_proxy_refreshing_for_testing) {
  if (!disable_proxy_refreshing_for_testing_) {
    // Refresh the proxy list immediately.
    RefreshProxyList();
  }
}

IpProtectionProxyListManagerImpl::~IpProtectionProxyListManagerImpl() = default;

bool IpProtectionProxyListManagerImpl::IsProxyListAvailable() {
  return have_fetched_proxy_list_;
}

const std::vector<std::string>& IpProtectionProxyListManagerImpl::ProxyList() {
  return proxy_list_;
}

void IpProtectionProxyListManagerImpl::RefreshProxyList() {
  if (fetching_proxy_list_ || !config_getter_) {
    return;
  }

  fetching_proxy_list_ = true;
  last_proxy_list_refresh_ = base::Time::Now();

  config_getter_->get()->GetProxyList(
      base::BindOnce(&IpProtectionProxyListManagerImpl::OnGotProxyList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IpProtectionProxyListManagerImpl::OnGotProxyList(
    const absl::optional<std::vector<std::string>>& proxy_list) {
  fetching_proxy_list_ = false;

  // If an error occurred fetching the proxy list, continue using the existing
  // proxy list, if any.
  if (proxy_list.has_value()) {
    proxy_list_ = *proxy_list;
    have_fetched_proxy_list_ = true;
  }

  // Schedule the next refresh. If this timer was already running, this will
  // reschedule it for the given time.
  if (!disable_proxy_refreshing_for_testing_) {
    base::TimeDelta delay =
        net::features::kIpPrivacyProxyListFetchInterval.Get();
    next_refresh_proxy_list_.Start(
        FROM_HERE, delay,
        base::BindOnce(&IpProtectionProxyListManagerImpl::RefreshProxyList,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (on_proxy_list_refreshed_for_testing_) {
    std::move(on_proxy_list_refreshed_for_testing_).Run();
  }
}

void IpProtectionProxyListManagerImpl::RequestRefreshProxyList() {
  // Do not refresh the list too frequently.
  base::TimeDelta minimum_age =
      net::features::kIpPrivacyProxyListMinFetchInterval.Get();

  if (base::Time::Now() - last_proxy_list_refresh_ < minimum_age) {
    return;
  }

  RefreshProxyList();
}

}  // namespace network
