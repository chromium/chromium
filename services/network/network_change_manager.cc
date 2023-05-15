// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_change_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_passive.h"

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
  client_remote.set_disconnect_handler(base::BindOnce(
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

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
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

  net::NetworkChangeNotifierPassive* notifier =
      static_cast<net::NetworkChangeNotifierPassive*>(
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

#if BUILDFLAG(IS_LINUX)
void NetworkChangeManager::BindNetworkInterfaceChangeListener(
    mojo::PendingAssociatedReceiver<mojom::NetworkInterfaceChangeListener>
        listener_receiver) {
  interface_change_listener_receiver_.Bind(std::move(listener_receiver));
}

// NetworkInterfaceChangeListener implementation:
void NetworkChangeManager::OnNetworkInterfacesChanged(
    mojom::NetworkInterfaceChangeParamsPtr change_params) {
  // network_change_notifier_ can be null in unit tests.
  if (!network_change_notifier_) {
    return;
  }

  net::NetworkChangeNotifierPassive* notifier =
      static_cast<net::NetworkChangeNotifierPassive*>(
          network_change_notifier_.get());

  notifier->GetAddressMapOwner()->GetAddressMapCacheLinux()->ApplyDiffs(
      change_params->address_map, change_params->online_links);
}
#endif  // BUILDFLAG(IS_LINUX)

size_t NetworkChangeManager::GetNumClientsForTesting() const {
  return clients_.size();
}

void NetworkChangeManager::NotificationPipeBroken(
    mojom::NetworkChangeManagerClient* client) {
  clients_.erase(base::ranges::find(
      clients_, client, &mojo::Remote<mojom::NetworkChangeManagerClient>::get));
}

void NetworkChangeManager::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  connection_type_ = mojom::ConnectionType(type);
  for (const auto& client : clients_) {
    client->OnNetworkChanged(connection_type_);
  }
}

}  // namespace network
