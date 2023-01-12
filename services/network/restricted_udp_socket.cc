// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_udp_socket.h"

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
  udp_socket_->Send(data, traffic_annotation_, std::move(callback));
}

}  // namespace network
