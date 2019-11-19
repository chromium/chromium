// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_CHANGE_MANAGER_H_
#define SERVICES_NETWORK_NETWORK_CHANGE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace network {

// Implementation of mojom::NetworkChangeManager. All accesses to this class are
// done through mojo on the main thread. This registers itself to receive
// broadcasts from net::NetworkChangeNotifier and rebroadcasts the notifications
// to mojom::NetworkChangeManagerClients through mojo pipes.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkChangeManager
    : public mojom::NetworkChangeManager,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // If |network_change_notifier| is not null, |this| will take ownership of it.
  // Otherwise, the global net::NetworkChangeNotifier will be used.
  explicit NetworkChangeManager(
      std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier);

  ~NetworkChangeManager() override;

  // Binds a NetworkChangeManager receiver to this object. Mojo messages
  // coming through the associated pipe will be served by this object.
  void AddReceiver(mojo::PendingReceiver<mojom::NetworkChangeManager> receiver);

  // mojom::NetworkChangeManager implementation:
  void RequestNotifications(
      mojo::PendingRemote<mojom::NetworkChangeManagerClient> client_remote)
      override;

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  void OnNetworkChanged(
      bool dns_changed,
      bool ip_address_changed,
      bool connection_type_changed,
      mojom::ConnectionType new_connection_type,
      bool connection_subtype_changed,
      mojom::ConnectionSubtype new_connection_subtype) override;
#endif

  size_t GetNumClientsForTesting() const;

 private:
  // Handles connection errors on notification pipes.
  void NotificationPipeBroken(mojom::NetworkChangeManagerClient* client);

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  mojo::ReceiverSet<mojom::NetworkChangeManager> receivers_;
  std::vector<mojo::Remote<mojom::NetworkChangeManagerClient>> clients_;
  mojom::ConnectionType connection_type_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeManager);
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_CHANGE_MANAGER_H_
