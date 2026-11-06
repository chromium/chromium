// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_CONFIG_SERVICE_MOJO_H_
#define SERVICES_NETWORK_PROXY_CONFIG_SERVICE_MOJO_H_

#include <optional>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "services/network/public/mojom/proxy_config.mojom.h"
#include "services/network/public/mojom/proxy_config_with_annotation.mojom.h"

namespace net {
class ProxyConfigWithAnnotation;
}

namespace network {

// ProxyConfigService that gets its proxy configuration over a Mojo pipe.
class COMPONENT_EXPORT(NETWORK_SERVICE) ProxyConfigServiceMojo
    : public net::ProxyConfigService,
      public mojom::ProxyConfigClient {
 public:
  // |proxy_config_client_receiver| is the Mojo pipe over which new
  // configurations are received. |initial_proxy_config| is the initial proxy
  // configuration. If mojo::NullReceiver(), waits for the initial configuration
  // over |proxy_config_client_receiver|. Either |proxy_config_client_receiver|
  // or |initial_proxy_config| must be non-null. If
  // |proxy_config_client_receiver| and |proxy_poller_client| are non-null,
  // calls into |proxy_poller_client| whenever OnLazyPoll() is invoked.
  explicit ProxyConfigServiceMojo(
      mojo::PendingReceiver<mojom::ProxyConfigClient>
          proxy_config_client_receiver,
      std::optional<net::ProxyConfigWithAnnotation> initial_proxy_config,
      mojo::PendingRemote<mojom::ProxyConfigPollerClient> proxy_poller_client);

  ProxyConfigServiceMojo(const ProxyConfigServiceMojo&) = delete;
  ProxyConfigServiceMojo& operator=(const ProxyConfigServiceMojo&) = delete;

  ~ProxyConfigServiceMojo() override;

  // net::ProxyConfigService implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfigWithAnnotation* config) override;
  void OnLazyPoll() override;

  // Flushes pending messages on `proxy_poller_client_` so unit tests can
  // deterministically observe sent or suppressed `OnLazyProxyConfigPoll()` IPCs
  // without spinning `RunUntilIdle()`.
  void FlushPollerForTesting();

 private:
  // mojom::ProxyConfigClient implementation:
  void OnProxyConfigUpdated(
      const net::ProxyConfigWithAnnotation& proxy_config) override;
  void FlushProxyConfig(FlushProxyConfigCallback callback) override;

  // Kept at the beginning of the class layout (immediately after the vtable
  // pointers and ahead of the large `config_` member) so all three fields read
  // by the `OnLazyPoll()` hot path reside in the same 64-byte L1 cache line
  // already loaded for virtual dispatch on `net::ProxyConfigService`.
  mojo::Remote<mojom::ProxyConfigPollerClient> proxy_poller_client_;

  // Minimum interval between consecutive `OnLazyProxyConfigPoll()` Mojo IPCs.
  // Cached once at construction time when `kRateLimitProxyConfigPolls` is
  // enabled; remains zero when disabled so the Control arm skips timekeeping.
  base::TimeDelta min_poll_interval_;

  // Earliest `TimeTicks` at which the next `OnLazyProxyConfigPoll()` IPC may be
  // sent when `min_poll_interval_` is positive. Initialized to `Min()` so the
  // first `OnLazyPoll()` call always passes `now < next_poll_time_` (`false`)
  // without requiring an extra `is_null()` branch on the hot path.
  base::TimeTicks next_poll_time_ = base::TimeTicks::Min();

  net::ProxyConfigWithAnnotation config_;
  bool config_pending_ = true;

  mojo::Receiver<mojom::ProxyConfigClient> receiver_{this};

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_CONFIG_SERVICE_MOJO_H_
