// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_RESTRICTED_UDP_SOCKET_H_
#define SERVICES_NETWORK_TEST_TEST_RESTRICTED_UDP_SOCKET_H_

#include "services/network/public/mojom/restricted_udp_socket.mojom.h"
#include "services/network/test/test_udp_socket.h"

namespace network {

class TestRestrictedUDPSocket : public mojom::RestrictedUDPSocket {
 public:
  explicit TestRestrictedUDPSocket(std::unique_ptr<TestUDPSocket> socket);

  ~TestRestrictedUDPSocket() override;

  TestRestrictedUDPSocket(const TestRestrictedUDPSocket&) = delete;
  TestRestrictedUDPSocket& operator=(const TestRestrictedUDPSocket&) = delete;

  // mojom::RestrictedUDPSocket:
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void Send(base::span<const uint8_t> data, SendCallback callback) override;
  void SendTo(base::span<const uint8_t> data,
              const net::HostPortPair& dest_addr,
              net::DnsQueryType dns_query_type,
              SendToCallback callback) override;

  TestUDPSocket* udp_socket() const { return udp_socket_.get(); }

 private:
  std::unique_ptr<TestUDPSocket> udp_socket_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_RESTRICTED_UDP_SOCKET_H_
