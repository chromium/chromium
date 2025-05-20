// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/network/connection_change_observer.h"

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"

namespace network {

ConnectionChangeObserver::ConnectionChangeObserver(
    mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
        observer,
    raw_ptr<NetworkContext> network_context)
    : network_context_(std::move(network_context)) {
  observer_.Bind(std::move(observer));
  observer_.set_disconnect_handler(base::BindOnce(
      &ConnectionChangeObserver::OnDisconnectEvent, base::Unretained(this)));
}

ConnectionChangeObserver::~ConnectionChangeObserver() = default;

void ConnectionChangeObserver::OnSessionClosed() {
  observer_->OnSessionClosed();
}

void ConnectionChangeObserver::OnConnectionFailed() {
  observer_->OnConnectionFailed();
}

void ConnectionChangeObserver::OnNetworkEvent(net::NetworkChangeEvent event) {
  observer_->OnNetworkEvent(event);
}

void ConnectionChangeObserver::OnDisconnectEvent() {
  // Remove `this` from the `NetworkContext` so that it can be destructed
  // when disconnecting.
  CHECK(network_context_);
  network_context_->RemoveConnectionChangeObserver(this);
}
}  // namespace network
