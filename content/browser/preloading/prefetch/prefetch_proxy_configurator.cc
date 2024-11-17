// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_proxy_configurator.h"

#include "base/barrier_closure.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/time/default_clock.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "url/gurl.h"

namespace content {

// static
std::unique_ptr<PrefetchProxyConfigurator>
PrefetchProxyConfigurator::MaybeCreatePrefetchProxyConfigurator(
    const GURL& proxy_url,
    const std::string& api_key) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchProxy)) {
    return nullptr;
  }

  if (!proxy_url.is_valid())
    return nullptr;

  return std::make_unique<PrefetchProxyConfigurator>(proxy_url, api_key);
}

PrefetchProxyConfigurator::PrefetchProxyConfigurator(const GURL& proxy_url,
                                                     const std::string& api_key)
    : prefetch_proxy_chain_(net::GetSchemeFromUriScheme(proxy_url.scheme()),
                            net::HostPortPair::FromURL(proxy_url)),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(proxy_url.is_valid());

  std::string server_experiment_group = PrefetchProxyServerExperimentGroup();
  std::string header_value =
      "key=" + api_key +
      (server_experiment_group != "" ? ",exp=" + server_experiment_group : "");

  connect_tunnel_headers_.SetHeader("chrome-tunnel", header_value);
}

PrefetchProxyConfigurator::~PrefetchProxyConfigurator() = default;

void PrefetchProxyConfigurator::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void PrefetchProxyConfigurator::AddCustomProxyConfigClient(
    mojo::Remote<network::mojom::CustomProxyConfigClient> config_client,
    base::OnceCallback<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_config_clients_.Add(std::move(config_client));
  UpdateCustomProxyConfig(std::move(callback));
}

void PrefetchProxyConfigurator::UpdateCustomProxyConfig(
    base::OnceCallback<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::RepeatingClosure repeating_closure =
      base::BarrierClosure(proxy_config_clients_.size(), std::move(callback));
  network::mojom::CustomProxyConfigPtr config = CreateCustomProxyConfig();
  for (auto& client : proxy_config_clients_) {
    client->OnCustomProxyConfigUpdated(config->Clone(), repeating_closure);
  }
}

network::mojom::CustomProxyConfigPtr
PrefetchProxyConfigurator::CreateCustomProxyConfig() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto config = network::mojom::CustomProxyConfig::New();
  config->rules.type =
      net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;

  // DIRECT is intentionally not added here because we want the proxy to always
  // be used in order to mask the user's IP address during the prerender.
  config->rules.proxies_for_https.AddProxyChain(prefetch_proxy_chain_);

  // This ensures that the user's set proxy is honored, although we also disable
  // the feature is such cases.
  config->should_override_existing_config = false;
  config->allow_non_idempotent_methods = false;
  config->connect_tunnel_headers = connect_tunnel_headers_;
  return config;
}

mojo::PendingRemote<network::mojom::CustomProxyConnectionObserver>
PrefetchProxyConfigurator::NewProxyConnectionObserverRemote() {
  mojo::PendingRemote<network::mojom::CustomProxyConnectionObserver>
      observer_remote;
  observer_receivers_.Add(this,
                          observer_remote.InitWithNewPipeAndPassReceiver());
  // The disconnect handler is intentionally not set since ReceiverSet manages
  // connection clean up on disconnect.
  return observer_remote;
}

void PrefetchProxyConfigurator::OnFallback(const net::ProxyChain& bad_chain,
                                           int net_error) {
  if (bad_chain != prefetch_proxy_chain_) {
    return;
  }

  base::UmaHistogramSparse("PrefetchProxy.Proxy.Fallback.NetError",
                           std::abs(net_error));

  OnTunnelProxyConnectionError(std::nullopt);
}

void PrefetchProxyConfigurator::OnTunnelHeadersReceived(
    const net::ProxyChain& proxy_chain,
    uint64_t chain_index,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  DCHECK(response_headers);

  if (proxy_chain != prefetch_proxy_chain_) {
    return;
  }

  base::UmaHistogramSparse("PrefetchProxy.Proxy.RespCode",
                           response_headers->response_code());

  if (response_headers->response_code() == net::HTTP_OK) {
    return;
  }

  std::string retry_after_string;
  if (response_headers->EnumerateHeader(nullptr, "Retry-After",
                                        &retry_after_string)) {
    base::TimeDelta retry_after;
    if (net::HttpUtil::ParseRetryAfterHeader(retry_after_string, clock_->Now(),
                                             &retry_after)) {
      OnTunnelProxyConnectionError(retry_after);
      return;
    }
  }

  OnTunnelProxyConnectionError(std::nullopt);
}

bool PrefetchProxyConfigurator::IsPrefetchProxyAvailable() const {
  if (!prefetch_proxy_not_available_until_) {
    return true;
  }

  return prefetch_proxy_not_available_until_.value() <= clock_->Now();
}

void PrefetchProxyConfigurator::OnTunnelProxyConnectionError(
    std::optional<base::TimeDelta> retry_after) {
  base::Time retry_proxy_at;
  if (retry_after) {
    retry_proxy_at = clock_->Now() + *retry_after;
  } else {
    // Pick a random value between 1-5 mins if the proxy didn't give us a
    // Retry-After value. The randomness will help ensure there is no sudden
    // wave of requests following a proxy error.
    retry_proxy_at = clock_->Now() + base::Seconds(base::RandInt(
                                         base::Time::kSecondsPerMinute,
                                         5 * base::Time::kSecondsPerMinute));
  }
  DCHECK(!retry_proxy_at.is_null());

  // If there is already a value in |prefetch_proxy_not_available_until_|,
  // probably due to some race, take the max.
  if (prefetch_proxy_not_available_until_) {
    prefetch_proxy_not_available_until_ =
        std::max(*prefetch_proxy_not_available_until_, retry_proxy_at);
  } else {
    prefetch_proxy_not_available_until_ = retry_proxy_at;
  }
  DCHECK(prefetch_proxy_not_available_until_);

  // TODO(crbug.com/40152136): Consider persisting to prefs.
}

}  // namespace content
