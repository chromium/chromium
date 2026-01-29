// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/socket_broker_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"
#include "net/test/gtest_util.h"
#include "services/network/public/cpp/socket_broker_client.h"
#include "services/network/public/mojom/socket_broker.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace network {

namespace {

template <typename Socket>
void ExpectIPSocket(std::unique_ptr<Socket> socket,
                    net::AddressFamily expect_address_family) {
  net::IPAddress bind_address =
      expect_address_family == net::ADDRESS_FAMILY_IPV4
          ? net::IPAddress::IPv4AllZeros()
          : net::IPAddress::IPv6AllZeros();
  EXPECT_EQ(socket->Bind(net::IPEndPoint(bind_address, 0u)), net::OK);
  net::IPEndPoint local_address;
  EXPECT_EQ(socket->GetLocalAddress(&local_address), net::OK);
  EXPECT_EQ(local_address.GetFamily(), expect_address_family);
}

void ExpectTcpSocket(network::TransferableSocket socket,
                     net::AddressFamily expect_address_family) {
  net::SocketDescriptor descriptor = socket.TakeSocket();
  EXPECT_NE(descriptor, net::kInvalidSocket);
  auto tcp_socket = net::TCPSocket::Create(nullptr, net::NetLogWithSource());
  EXPECT_EQ(tcp_socket->AdoptUnconnectedSocket(descriptor), net::OK);
  EXPECT_TRUE(tcp_socket->IsValid());
  ExpectIPSocket(std::move(tcp_socket), expect_address_family);
}

void ExpectUdpSocket(network::TransferableSocket socket,
                     net::AddressFamily expect_address_family) {
  net::SocketDescriptor descriptor = socket.TakeSocket();
  EXPECT_NE(descriptor, net::kInvalidSocket);
  auto udp_socket = std::make_unique<net::UDPSocket>(
      net::DatagramSocket::RANDOM_BIND, net::NetLogWithSource());
  EXPECT_EQ(udp_socket->AdoptOpenedSocket(expect_address_family, descriptor),
            net::OK);
  ExpectIPSocket(std::move(udp_socket), expect_address_family);
}

class SocketBrokerImplTest : public ::testing::Test {
 public:
  SocketBrokerImplTest()
      : socket_broker_client_(socket_broker_impl_.BindNewRemote()) {}

  SocketBrokerClient& socket_broker_client() { return socket_broker_client_; }

 private:
  base::test::TaskEnvironment task_environment_;

  SocketBrokerImpl socket_broker_impl_;
  SocketBrokerClient socket_broker_client_;
};

TEST_F(SocketBrokerImplTest, TcpIPV4) {
  base::test::TestFuture<network::TransferableSocket, int> ipv4_future;
  socket_broker_client().CreateTcpSocket(net::ADDRESS_FAMILY_IPV4,
                                         ipv4_future.GetCallback());
  auto [socket1, rv1] = ipv4_future.Take();
  EXPECT_EQ(rv1, net::OK);
  ExpectTcpSocket(std::move(socket1), net::ADDRESS_FAMILY_IPV4);
}

TEST_F(SocketBrokerImplTest, TcpIPv6) {
  base::test::TestFuture<network::TransferableSocket, int> ipv6_future;
  socket_broker_client().CreateTcpSocket(net::ADDRESS_FAMILY_IPV6,
                                         ipv6_future.GetCallback());
  auto [socket2, rv2] = ipv6_future.Take();
  EXPECT_EQ(rv2, net::OK);
  ExpectTcpSocket(std::move(socket2), net::ADDRESS_FAMILY_IPV6);
}

TEST_F(SocketBrokerImplTest, UdpIPV4) {
  base::test::TestFuture<network::TransferableSocket, int> ipv4_future;
  socket_broker_client().CreateUdpSocket(net::ADDRESS_FAMILY_IPV4,
                                         ipv4_future.GetCallback());
  auto [socket1, rv1] = ipv4_future.Take();
  EXPECT_EQ(rv1, net::OK);
  ExpectUdpSocket(std::move(socket1), net::ADDRESS_FAMILY_IPV4);
}

TEST_F(SocketBrokerImplTest, UdpIPv6) {
  base::test::TestFuture<network::TransferableSocket, int> ipv6_future;
  socket_broker_client().CreateUdpSocket(net::ADDRESS_FAMILY_IPV6,
                                         ipv6_future.GetCallback());
  auto [socket2, rv2] = ipv6_future.Take();
  EXPECT_EQ(rv2, net::OK);
  ExpectUdpSocket(std::move(socket2), net::ADDRESS_FAMILY_IPV6);
}

}  // namespace

}  // namespace network
