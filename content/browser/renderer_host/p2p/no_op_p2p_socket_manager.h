// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_NO_OP_P2P_SOCKET_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_NO_OP_P2P_SOCKET_MANAGER_H_

#include <optional>
#include <string>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/p2p.mojom.h"

namespace content {

// A P2PSocketManager that never performs any networking. It is bound in place
// of the real (network service backed) P2PSocketManager for documents whose
// Connection Allowlist blocks WebRTC.
//
// Binding a no-op implementation (rather than dropping the receiver or leaving
// the interface unregistered) lets an unmodified renderer fail gracefully:
// - The pipe stays connected, so the renderer's P2PSocketDispatcher does not
//   enter its reconnect loop.
// - An empty network list is reported, so ICE gathering is allowed to proceed.
// - All reply callbacks are run, so no request hangs.
class CONTENT_EXPORT NoOpP2PSocketManager
    : public network::mojom::P2PSocketManager {
 public:
  // Creates a self-owned NoOpP2PSocketManager bound to `receiver`. It is
  // destroyed when the renderer closes the pipe.
  static void Create(
      mojo::PendingReceiver<network::mojom::P2PSocketManager> receiver);

  NoOpP2PSocketManager();
  NoOpP2PSocketManager(const NoOpP2PSocketManager&) = delete;
  NoOpP2PSocketManager& operator=(const NoOpP2PSocketManager&) = delete;
  ~NoOpP2PSocketManager() override;

  // network::mojom::P2PSocketManager:
  void StartNetworkNotifications(
      mojo::PendingRemote<network::mojom::P2PNetworkNotificationClient> client)
      override;
  void GetHostAddress(const std::string& host_name,
                      std::optional<net::AddressFamily> address_family,
                      bool enable_mdns,
                      GetHostAddressCallback callback) override;
  void CreateSocket(
      network::P2PSocketType type,
      const net::IPEndPoint& local_address,
      const network::P2PPortRange& port_range,
      const network::P2PHostAndIPEndPoint& remote_address,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const std::optional<base::UnguessableToken>& devtools_token,
      mojo::PendingRemote<network::mojom::P2PSocketClient> client,
      mojo::PendingReceiver<network::mojom::P2PSocket> receiver) override;

 private:
  // Kept alive for the lifetime of this object so that the renderer does not
  // observe a disconnect on its notification client.
  mojo::Remote<network::mojom::P2PNetworkNotificationClient>
      network_notification_client_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_NO_OP_P2P_SOCKET_MANAGER_H_
