// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_UDP_SOCKET_IMPL_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_UDP_SOCKET_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace content {

// Forwards requests from the Renderer to the connected UDPSocket.
// We do not expose the UDPSocket directly to the Renderer, as that
// would allow a compromised Renderer to contact other end points.
class CONTENT_EXPORT DirectUDPSocketImpl
    : public network::mojom::RestrictedUDPSocket {
 public:
  using ConnectCallback = network::mojom::UDPSocket::ConnectCallback;

  DirectUDPSocketImpl(
      network::mojom::NetworkContext* network_context,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener);
  ~DirectUDPSocketImpl() override;

  // Connect should be called once, immediately after construction.
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr options,
               ConnectCallback callback);

  // blink::mojom::RestrictedUDPSocket:
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void Send(base::span<const uint8_t> data, SendCallback callback) override;

 private:
  void OnDisconnect();

  mojo::Remote<network::mojom::UDPSocket> remote_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_UDP_SOCKET_IMPL_H_
