// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_PROXY_LIST_MANAGER_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_PROXY_LIST_MANAGER_IMPL_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/ip_protection_proxy_list_manager.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

// An implementation of IpProtectionProxyListManager that populates itself using
// a passed in IpProtectionConfigGetter pointer from the cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionProxyListManagerImpl
    : public IpProtectionProxyListManager {
 public:
  explicit IpProtectionProxyListManagerImpl(
      mojo::Remote<network::mojom::IpProtectionConfigGetter>* config_getter,
      bool disable_proxy_refreshing_for_testing = false);
  ~IpProtectionProxyListManagerImpl() override;

  // IpProtectionProxyListManager implementation.
  bool IsProxyListAvailable() override;
  const std::vector<std::string>& ProxyList() override;
  void RequestRefreshProxyList() override;

  // Set a callback to occur when the proxy list has been refreshed.
  void SetOnProxyListRefreshedForTesting(
      base::OnceClosure on_proxy_list_refreshed) {
    on_proxy_list_refreshed_for_testing_ = std::move(on_proxy_list_refreshed);
  }

  // Trigger a proxy list refresh.
  void EnableProxyListRefreshingForTesting() {
    disable_proxy_refreshing_for_testing_ = false;
    RefreshProxyList();
  }

 private:
  void RefreshProxyList();
  void OnGotProxyList(const absl::optional<std::vector<std::string>>&);

  // Latest fetched proxy list.
  std::vector<std::string> proxy_list_;

  // True if an invocation of `config_getter_.GetProxyList()` is
  // outstanding.
  bool fetching_proxy_list_ = false;

  // True if the proxy list has been fetched at least once.
  bool have_fetched_proxy_list_ = false;

  // Source of proxy list, when needed.
  raw_ptr<mojo::Remote<network::mojom::IpProtectionConfigGetter>>
      config_getter_;

  // The last time this instance began refreshing the proxy list.
  base::Time last_proxy_list_refresh_;

  // A timer to run `RefreshProxyList()` when necessary.
  base::OneShotTimer next_refresh_proxy_list_;

  // A callback triggered when an asynchronous proxy-list refresh is complete,
  // for use in testing.
  base::OnceClosure on_proxy_list_refreshed_for_testing_;

  // If true, do not try to automatically refresh the proxy list.
  bool disable_proxy_refreshing_for_testing_ = false;

  base::WeakPtrFactory<IpProtectionProxyListManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_PROXY_LIST_MANAGER_IMPL_H_
