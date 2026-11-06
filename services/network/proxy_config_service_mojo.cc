// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_config_service_mojo.h"

#include <utility>

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "services/network/public/cpp/features.h"

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
      // NullRemote() is passed in unit tests and on platforms where the
      // browser's ProxyConfigService does not use polling (e.g. macOS, Linux,
      // ChromeOS, Android). Return before querying kRateLimitProxyConfigPolls
      // so non-polling platforms never activate the field trial or dilute A/B
      // experiment metrics.
      return;
    }
    proxy_poller_client_.Bind(std::move(proxy_poller_client));

    // Cache the feature state and minimum interval once at construction time
    // (one instance per NetworkContext) so the OnLazyPoll() hot path performs
    // zero base::FeatureList or parameter map lookups during proxy resolution.
    if (base::FeatureList::IsEnabled(features::kRateLimitProxyConfigPolls)) {
      min_poll_interval_ = features::kRateLimitProxyConfigPollsInterval.Get();
    }
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
  if (!proxy_poller_client_) {
    return;
  }

  // `ConfiguredProxyResolutionService::ResolveProxy()` invokes `OnLazyPoll()`
  // synchronously on every URL and socket proxy resolution. When rate-limiting
  // is enabled (`min_poll_interval_.is_positive()`), compare against the
  // precomputed `next_poll_time_` deadline (a plain 64-bit integer comparison,
  // avoiding clamped `TimeDelta` subtraction on the fast return path) to
  // suppress redundant cross-process IPCs and Browser UI thread wakeups during
  // request bursts. When disabled, `min_poll_interval_` is zero and Control
  // avoids calling `base::TimeTicks::Now()`.
  if (min_poll_interval_.is_positive()) {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (now < next_poll_time_) [[likely]] {
      return;
    }
    next_poll_time_ = now + min_poll_interval_;
  }

  proxy_poller_client_->OnLazyProxyConfigPoll();
}

void ProxyConfigServiceMojo::FlushPollerForTesting() {
  if (proxy_poller_client_) {
    proxy_poller_client_.FlushForTesting();  // IN-TEST
  }
}

}  // namespace network
