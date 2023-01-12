// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESTRICTED_UDP_SOCKET_H_
#define SERVICES_NETWORK_RESTRICTED_UDP_SOCKET_H_

#include "base/component_export.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom.h"

namespace network {

class UDPSocket;

// Forwards requests from the Renderer to the connected UDPSocket.
// We do not expose the UDPSocket directly to the Renderer, as that
// would allow a compromised Renderer to contact other end points.
class COMPONENT_EXPORT(NETWORK_SERVICE) RestrictedUDPSocket
    : public network::mojom::RestrictedUDPSocket {
 public:
  RestrictedUDPSocket(
      std::unique_ptr<UDPSocket> udp_socket,
      net::MutableNetworkTrafficAnnotationTag traffic_annotation);
  ~RestrictedUDPSocket() override;

  // blink::mojom::RestrictedUDPSocket:
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void Send(base::span<const uint8_t> data, SendCallback callback) override;

 private:
  std::unique_ptr<UDPSocket> udp_socket_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESTRICTED_UDP_SOCKET_H_
