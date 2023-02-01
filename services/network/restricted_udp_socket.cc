// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_udp_socket.h"

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "services/network/udp_socket.h"

namespace network {

RestrictedUDPSocket::RestrictedUDPSocket(
    std::unique_ptr<UDPSocket> udp_socket,
    net::MutableNetworkTrafficAnnotationTag traffic_annotation)
    : udp_socket_(std::move(udp_socket)),
      traffic_annotation_(std::move(traffic_annotation)) {}

RestrictedUDPSocket::~RestrictedUDPSocket() = default;

void RestrictedUDPSocket::ReceiveMore(uint32_t num_additional_datagrams) {
  udp_socket_->ReceiveMore(num_additional_datagrams);
}

void RestrictedUDPSocket::Send(base::span<const uint8_t> data,
                               SendCallback callback) {
  udp_socket_->Send(std::move(data), traffic_annotation_, std::move(callback));
}

void RestrictedUDPSocket::SendTo(base::span<const uint8_t> data,
                                 const net::HostPortPair& dest_addr,
                                 SendToCallback callback) {
  // If a raw IP address is supplied, call SendTo() immediately.
  if (net::IPAddress address; address.AssignFromIPLiteral(dest_addr.host())) {
    udp_socket_->SendTo(net::IPEndPoint(std::move(address), dest_addr.port()),
                        data, traffic_annotation_, std::move(callback));
    return;
  }

  // Arbitrary addresses that require DNS resolution are currently unsupported.
  std::move(callback).Run(net::ERR_NOT_IMPLEMENTED);
}

}  // namespace network
