// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/no_op_p2p_socket_manager.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/ip_address.h"

namespace content {

// static
void NoOpP2PSocketManager::Create(
    mojo::PendingReceiver<network::mojom::P2PSocketManager> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<NoOpP2PSocketManager>(),
                              std::move(receiver));
}

NoOpP2PSocketManager::NoOpP2PSocketManager() = default;
NoOpP2PSocketManager::~NoOpP2PSocketManager() = default;

void NoOpP2PSocketManager::StartNetworkNotifications(
    mojo::PendingRemote<network::mojom::P2PNetworkNotificationClient> client) {
  network_notification_client_.reset();
  network_notification_client_.Bind(std::move(client));
  // Report an empty network list so nothing is waiting on network changes.
  network_notification_client_->NetworkListChanged(
      /*networks=*/{}, /*default_ipv4_local_address=*/net::IPAddress(),
      /*default_ipv6_local_address=*/net::IPAddress());
}

void NoOpP2PSocketManager::GetHostAddress(
    const std::string& host_name,
    std::optional<net::AddressFamily> address_family,
    bool enable_mdns,
    GetHostAddressCallback callback) {
  // Never resolve anything.
  std::move(callback).Run(/*addresses=*/{});
}

void NoOpP2PSocketManager::CreateSocket(
    network::P2PSocketType type,
    const net::IPEndPoint& local_address,
    const network::P2PPortRange& port_range,
    const network::P2PHostAndIPEndPoint& remote_address,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    const std::optional<base::UnguessableToken>& devtools_token,
    mojo::PendingRemote<network::mojom::P2PSocketClient> client,
    mojo::PendingReceiver<network::mojom::P2PSocket> receiver) {
  // Never create a socket. Letting these pending pipes go out of scope should
  // destroy them and treat them as disconnected.
}

}  // namespace content
