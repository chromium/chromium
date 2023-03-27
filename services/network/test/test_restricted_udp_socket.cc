// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_restricted_udp_socket.h"

#include "base/notreached.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace network {

TestRestrictedUDPSocket::TestRestrictedUDPSocket(
    std::unique_ptr<TestUDPSocket> udp_socket)
    : udp_socket_(std::move(udp_socket)) {}

TestRestrictedUDPSocket::~TestRestrictedUDPSocket() = default;

void TestRestrictedUDPSocket::ReceiveMore(uint32_t num_additional_datagrams) {
  udp_socket_->ReceiveMore(num_additional_datagrams);
}

void TestRestrictedUDPSocket::Send(base::span<const uint8_t> data,
                                   SendCallback callback) {
  udp_socket_->Send(
      data,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      std::move(callback));
}

void TestRestrictedUDPSocket::SendTo(base::span<const uint8_t> data,
                                     const net::HostPortPair& dest_addr,
                                     net::DnsQueryType dns_query_type,
                                     SendToCallback callback) {
  if (net::IPAddress address; address.AssignFromIPLiteral(dest_addr.host())) {
    udp_socket_->SendTo(
        net::IPEndPoint(std::move(address), dest_addr.port()), data,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::move(callback));
    return;
  }

  NOTIMPLEMENTED();
}

}  // namespace network
