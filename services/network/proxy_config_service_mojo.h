// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_CONFIG_SERVICE_MOJO_H_
#define SERVICES_NETWORK_PROXY_CONFIG_SERVICE_MOJO_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
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
      base::Optional<net::ProxyConfigWithAnnotation> initial_proxy_config,
      mojo::PendingRemote<mojom::ProxyConfigPollerClient> proxy_poller_client);
  ~ProxyConfigServiceMojo() override;

  // net::ProxyConfigService implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfigWithAnnotation* config) override;
  void OnLazyPoll() override;

 private:
  // mojom::ProxyConfigClient implementation:
  void OnProxyConfigUpdated(
      const net::ProxyConfigWithAnnotation& proxy_config) override;
  void FlushProxyConfig(FlushProxyConfigCallback callback) override;

  mojo::Remote<mojom::ProxyConfigPollerClient> proxy_poller_client_;

  net::ProxyConfigWithAnnotation config_;
  bool config_pending_ = true;

  mojo::Receiver<mojom::ProxyConfigClient> receiver_{this};

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceMojo);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_CONFIG_SERVICE_MOJO_H_
