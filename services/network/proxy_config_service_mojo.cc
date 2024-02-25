// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_config_service_mojo.h"

#include <utility>

#include "base/observer_list.h"

namespace network {

ProxyConfigServiceMojo::ProxyConfigServiceMojo(
    mojo::PendingReceiver<mojom::ProxyConfigClient>
        proxy_config_client_receiver,
    std::optional<net::ProxyConfigWithAnnotation> initial_proxy_config,
    mojo::PendingRemote<mojom::ProxyConfigPollerClient> proxy_poller_client) {
  DCHECK(initial_proxy_config || proxy_config_client_receiver.is_valid());

  if (initial_proxy_config)
    OnProxyConfigUpdated(*initial_proxy_config);

  if (proxy_config_client_receiver.is_valid()) {
    receiver_.Bind(std::move(proxy_config_client_receiver));
    // Only use the |proxy_poller_client| if there's a
    // |proxy_config_client_receiver|.
    if (!proxy_poller_client) {
      // NullRemote() could be passed in unit tests. In that case, it can't be
      // bound.
      return;
    }
    proxy_poller_client_.Bind(std::move(proxy_poller_client));
  }
}

ProxyConfigServiceMojo::~ProxyConfigServiceMojo() {}

void ProxyConfigServiceMojo::OnProxyConfigUpdated(
    const net::ProxyConfigWithAnnotation& proxy_config) {
  // Do nothing if the proxy configuration is unchanged.
  if (!config_pending_ && config_.value().Equals(proxy_config.value()))
    return;

  config_pending_ = false;
  config_ = proxy_config;

  for (auto& observer : observers_)
    observer.OnProxyConfigChanged(config_, CONFIG_VALID);
}

void ProxyConfigServiceMojo::FlushProxyConfig(
    FlushProxyConfigCallback callback) {
  std::move(callback).Run();
}

void ProxyConfigServiceMojo::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProxyConfigServiceMojo::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability
ProxyConfigServiceMojo::GetLatestProxyConfig(
    net::ProxyConfigWithAnnotation* config) {
  if (config_pending_) {
    *config = net::ProxyConfigWithAnnotation();
    return CONFIG_PENDING;
  }
  *config = config_;
  return CONFIG_VALID;
}

void ProxyConfigServiceMojo::OnLazyPoll() {
  // TODO(mmenke): These should either be rate limited, or the other process
  // should use another signal of activity.
  if (proxy_poller_client_)
    proxy_poller_client_->OnLazyProxyConfigPoll();
}

}  // namespace network
