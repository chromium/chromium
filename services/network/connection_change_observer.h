// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CONNECTION_CHANGE_OBSERVER_H_
#define SERVICES_NETWORK_CONNECTION_CHANGE_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) ConnectionChangeObserver
    : public net::ConnectionChangeNotifier::Observer {
 public:
  ConnectionChangeObserver(
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          observer,
      raw_ptr<NetworkContext> network_context);
  ~ConnectionChangeObserver() override;

  // net::ConnectionChangeNotifier::Observer methods.
  void OnSessionClosed() override;
  void OnConnectionFailed() override;
  void OnNetworkEvent(net::NetworkChangeEvent event) override;

 private:
  // called when the underlying mojo pipe has been disconnected.
  void OnDisconnectEvent();

  mojo::Remote<network::mojom::ConnectionChangeObserverClient> observer_;

  raw_ptr<NetworkContext> network_context_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_CONNECTION_CHANGE_OBSERVER_H_
