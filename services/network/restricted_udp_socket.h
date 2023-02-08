// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESTRICTED_UDP_SOCKET_H_
#define SERVICES_NETWORK_RESTRICTED_UDP_SOCKET_H_

#include "base/component_export.h"
#include "net/base/address_list.h"
#include "net/base/net_error_details.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom.h"

namespace network {

class UDPSocket;
class SimpleHostResolver;

// Forwards requests from the Renderer to the connected UDPSocket.
// We do not expose the UDPSocket directly to the Renderer, as that
// would allow a compromised Renderer to contact other end points.
class COMPONENT_EXPORT(NETWORK_SERVICE) RestrictedUDPSocket
    : public mojom::RestrictedUDPSocket {
 public:
  RestrictedUDPSocket(
      std::unique_ptr<UDPSocket> udp_socket,
      net::MutableNetworkTrafficAnnotationTag traffic_annotation,
      std::unique_ptr<SimpleHostResolver> resolver);
  ~RestrictedUDPSocket() override;

  // blink::mojom::RestrictedUDPSocket:
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void Send(base::span<const uint8_t> data, SendCallback callback) override;
  void SendTo(base::span<const uint8_t> data,
              const net::HostPortPair& dest_addr,
              SendToCallback callback) override;

 private:
  void OnResolveCompleteForSendTo(
      std::vector<uint8_t> data,
      SendToCallback callback,
      int result,
      const net::ResolveErrorInfo&,
      const absl::optional<net::AddressList>& resolved_addresses,
      const absl::optional<net::HostResolverEndpointResults>&);

  std::unique_ptr<UDPSocket> udp_socket_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  std::unique_ptr<SimpleHostResolver> resolver_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESTRICTED_UDP_SOCKET_H_
