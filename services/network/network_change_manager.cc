// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_change_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_posix.h"

namespace network {

NetworkChangeManager::NetworkChangeManager(
    std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier)
    : network_change_notifier_(std::move(network_change_notifier)) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  connection_type_ =
      mojom::ConnectionType(net::NetworkChangeNotifier::GetConnectionType());
}

NetworkChangeManager::~NetworkChangeManager() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void NetworkChangeManager::AddReceiver(
    mojo::PendingReceiver<mojom::NetworkChangeManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NetworkChangeManager::RequestNotifications(
    mojo::PendingRemote<mojom::NetworkChangeManagerClient>
        client_pending_remote) {
  mojo::Remote<mojom::NetworkChangeManagerClient> client_remote(
      std::move(client_pending_remote));
  client_remote.set_disconnect_handler(base::Bind(
      &NetworkChangeManager::NotificationPipeBroken,
      // base::Unretained is safe as destruction of the
      // NetworkChangeManager will also destroy the
      // |clients_| list (which this object will be
      // inserted into, below), which will destroy the
      // client_remote, rendering this callback moot.
      base::Unretained(this), base::Unretained(client_remote.get())));
  client_remote->OnInitialConnectionType(connection_type_);
  clients_.push_back(std::move(client_remote));
}

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
void NetworkChangeManager::OnNetworkChanged(
    bool dns_changed,
    bool ip_address_changed,
    bool connection_type_changed,
    mojom::ConnectionType new_connection_type,
    bool connection_subtype_changed,
    mojom::ConnectionSubtype new_connection_subtype) {
  // network_change_notifier_ can be null in unit tests.
  if (!network_change_notifier_)
    return;

  net::NetworkChangeNotifierPosix* notifier =
      static_cast<net::NetworkChangeNotifierPosix*>(
          network_change_notifier_.get());
  if (dns_changed)
    notifier->OnDNSChanged();
  if (ip_address_changed)
    notifier->OnIPAddressChanged();
  if (connection_type_changed) {
    notifier->OnConnectionChanged(
        net::NetworkChangeNotifier::ConnectionType(new_connection_type));
  }
  if (connection_type_changed || connection_subtype_changed) {
    notifier->OnConnectionSubtypeChanged(
        net::NetworkChangeNotifier::ConnectionType(new_connection_type),
        net::NetworkChangeNotifier::ConnectionSubtype(new_connection_subtype));
  }
}
#endif

size_t NetworkChangeManager::GetNumClientsForTesting() const {
  return clients_.size();
}

void NetworkChangeManager::NotificationPipeBroken(
    mojom::NetworkChangeManagerClient* client) {
  clients_.erase(std::find_if(
      clients_.begin(), clients_.end(),
      [client](mojo::Remote<mojom::NetworkChangeManagerClient>& remote) {
        return remote.get() == client;
      }));
}

void NetworkChangeManager::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  connection_type_ = mojom::ConnectionType(type);
  for (const auto& client : clients_) {
    client->OnNetworkChanged(connection_type_);
  }
}

}  // namespace network
