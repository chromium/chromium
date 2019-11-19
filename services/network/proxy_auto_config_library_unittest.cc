// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_auto_config_library.h"

#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/datagram_client_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

// Helper for verifying whether the address list returned by myIpAddress() /
// myIpAddressEx() looks correct.
void VerifyActualMyIpAddresses(const net::IPAddressList& test_list) {
  // Enumerate all of the IP addresses for the system (skipping loopback and
  // link-local ones). This is used as a reference implementation to check
  // whether |test_list| (which was obtained using a different strategy) looks
  // correct.
  std::set<net::IPAddress> candidates;
  net::NetworkInterfaceList networks;
  net::GetNetworkList(&networks, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES);
  for (const auto& network : networks) {
    if (network.address.IsLinkLocal() || network.address.IsLoopback())
      continue;
    candidates.insert(network.address);
  }

  // Ordinarily the machine running this test will have an IP address. However
  // for some bot configurations (notably Android) that may not be the case.
  EXPECT_EQ(candidates.empty(), test_list.empty());

  // |test_list| should be a subset of |candidates|.
  for (const auto& ip : test_list)
    EXPECT_EQ(1u, candidates.count(ip));
}

// Tests for PacMyIpAddress() and PacMyIpAddressEx().
TEST(PacLibraryTest, ActualPacMyIpAddress) {
  auto my_ip_addresses = PacMyIpAddress();

  VerifyActualMyIpAddresses(my_ip_addresses);
}

TEST(PacLibraryTest, ActualPacMyIpAddressEx) {
  VerifyActualMyIpAddresses(PacMyIpAddressEx());
}

net::IPAddress CreateIPAddress(base::StringPiece literal) {
  net::IPAddress result;
  if (!result.AssignFromIPLiteral(literal)) {
    ADD_FAILURE() << "Failed parsing IP: " << literal;
    return net::IPAddress();
  }
  return result;
}

net::AddressList CreateAddressList(
    const std::vector<base::StringPiece>& ip_literals) {
  net::AddressList result;
  for (const auto& ip : ip_literals)
    result.push_back(net::IPEndPoint(CreateIPAddress(ip), 8080));
  return result;
}

class MockUDPSocket : public net::DatagramClientSocket {
 public:
  MockUDPSocket(const net::IPAddress& peer_ip,
                const net::IPAddress& local_ip,
                net::Error connect_error)
      : peer_ip_(peer_ip), local_ip_(local_ip), connect_error_(connect_error) {}

  ~MockUDPSocket() override = default;

  // Socket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    ADD_FAILURE() << "Called Read()";
    return net::ERR_UNEXPECTED;
  }
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    ADD_FAILURE() << "Called Read()";
    return net::ERR_UNEXPECTED;
  }
  int SetReceiveBufferSize(int32_t size) override {
    ADD_FAILURE() << "Called SetReceiveBufferSize()";
    return net::ERR_UNEXPECTED;
  }
  int SetSendBufferSize(int32_t size) override {
    ADD_FAILURE() << "Called SetSendBufferSize()";
    return net::ERR_UNEXPECTED;
  }

  // net::DatagramSocket implementation.
  void Close() override { ADD_FAILURE() << "Called Close()"; }
  int GetPeerAddress(net::IPEndPoint* address) const override {
    ADD_FAILURE() << "Called GetPeerAddress()";
    return net::ERR_UNEXPECTED;
  }
  int GetLocalAddress(net::IPEndPoint* address) const override {
    if (connect_error_ != net::OK)
      return connect_error_;

    *address = net::IPEndPoint(local_ip_, 8080);
    return net::OK;
  }
  void UseNonBlockingIO() override {
    ADD_FAILURE() << "Called UseNonBlockingIO()";
  }
  int SetDoNotFragment() override {
    ADD_FAILURE() << "Called SetDoNotFragment()";
    return net::ERR_UNEXPECTED;
  }
  void SetMsgConfirm(bool confirm) override {
    ADD_FAILURE() << "Called SetMsgConfirm()";
  }
  const net::NetLogWithSource& NetLog() const override {
    ADD_FAILURE() << "Called net::NetLog()";
    return net_log_;
  }

  // net::DatagramClientSocket implementation.
  int Connect(const net::IPEndPoint& address) override {
    EXPECT_EQ(peer_ip_.ToString(), address.address().ToString());
    return connect_error_;
  }
  int ConnectUsingNetwork(net::NetworkChangeNotifier::NetworkHandle network,
                          const net::IPEndPoint& address) override {
    ADD_FAILURE() << "Called ConnectUsingNetwork()";
    return net::ERR_UNEXPECTED;
  }
  int ConnectUsingDefaultNetwork(const net::IPEndPoint& address) override {
    ADD_FAILURE() << "Called ConnectUsingDefaultNetwork()";
    return net::ERR_UNEXPECTED;
  }
  net::NetworkChangeNotifier::NetworkHandle GetBoundNetwork() const override {
    ADD_FAILURE() << "Called GetBoundNetwork()";
    return network_;
  }
  void ApplySocketTag(const net::SocketTag& tag) override {
    ADD_FAILURE() << "Called ApplySocketTag()";
  }
  int WriteAsync(
      net::DatagramBuffers buffers,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    ADD_FAILURE() << "Called WriteAsync()";
    return net::ERR_UNEXPECTED;
  }
  int WriteAsync(
      const char* buffer,
      size_t buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    ADD_FAILURE() << "Called WriteAsync()";
    return net::ERR_UNEXPECTED;
  }
  net::DatagramBuffers GetUnwrittenBuffers() override {
    ADD_FAILURE() << "Called GetUnwrittenBuffers()";
    return net::DatagramBuffers();
  }
  void SetWriteAsyncEnabled(bool enabled) override {
    ADD_FAILURE() << "Called SetWriteAsyncEnabled()";
  }
  void SetMaxPacketSize(size_t max_packet_size) override {
    ADD_FAILURE() << "Called SetWriteAsyncEnabled()";
  }
  bool WriteAsyncEnabled() override {
    ADD_FAILURE() << "Called WriteAsyncEnabled()";
    return false;
  }
  void SetWriteMultiCoreEnabled(bool enabled) override {
    ADD_FAILURE() << "Called SetWriteMultiCoreEnabled()";
  }
  void SetSendmmsgEnabled(bool enabled) override {
    ADD_FAILURE() << "Called SetSendmmsgEnabled()";
  }
  void SetWriteBatchingActive(bool active) override {
    ADD_FAILURE() << "Called SetWriteBatchingActive()";
  }
  int SetMulticastInterface(uint32_t interface_index) override {
    ADD_FAILURE() << "Called SetMulticastInterface()";
    return net::ERR_UNEXPECTED;
  }

 private:
  net::NetLogWithSource net_log_;
  net::NetworkChangeNotifier::NetworkHandle network_;

  net::IPAddress peer_ip_;
  net::IPAddress local_ip_;
  net::Error connect_error_;

  DISALLOW_COPY_AND_ASSIGN(MockUDPSocket);
};

class MockSocketFactory : public net::ClientSocketFactory {
 public:
  MockSocketFactory() = default;

  void AddUDPConnectSuccess(base::StringPiece peer_ip_literal,
                            base::StringPiece local_ip_literal) {
    auto peer_ip = CreateIPAddress(peer_ip_literal);
    auto local_ip = CreateIPAddress(local_ip_literal);

    // The address family of local and peer IP must match.
    ASSERT_EQ(peer_ip.size(), local_ip.size());

    udp_sockets_.push_back(
        std::make_unique<MockUDPSocket>(peer_ip, local_ip, net::OK));
  }

  void AddUDPConnectFailure(base::StringPiece peer_ip) {
    udp_sockets_.push_back(std::make_unique<MockUDPSocket>(
        CreateIPAddress(peer_ip), net::IPAddress(),
        net::ERR_ADDRESS_UNREACHABLE));
  }

  ~MockSocketFactory() override {
    EXPECT_EQ(0u, udp_sockets_.size())
        << "Not all of the mock sockets were consumed.";
  }

  // net::ClientSocketFactory
  std::unique_ptr<net::DatagramClientSocket> CreateDatagramClientSocket(
      net::DatagramSocket::BindType bind_type,
      net::NetLog* net_log,
      const net::NetLogSource& source) override {
    if (udp_sockets_.empty()) {
      ADD_FAILURE() << "Not enough mock UDP sockets";
      return nullptr;
    }

    auto result = std::move(udp_sockets_.front());
    udp_sockets_.erase(udp_sockets_.begin());
    return result;
  }
  std::unique_ptr<net::TransportClientSocket> CreateTransportClientSocket(
      const net::AddressList& addresses,
      std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher,
      net::NetLog* net_log,
      const net::NetLogSource& source) override {
    ADD_FAILURE() << "Called CreateTransportClientSocket()";
    return nullptr;
  }
  std::unique_ptr<net::SSLClientSocket> CreateSSLClientSocket(
      net::SSLClientContext* context,
      std::unique_ptr<net::StreamSocket> stream_socket,
      const net::HostPortPair& host_and_port,
      const net::SSLConfig& ssl_config) override {
    ADD_FAILURE() << "Called CreateSSLClientSocket()";
    return nullptr;
  }
  std::unique_ptr<net::ProxyClientSocket> CreateProxyClientSocket(
      std::unique_ptr<net::StreamSocket> stream_socket,
      const std::string& user_agent,
      const net::HostPortPair& endpoint,
      const net::ProxyServer& proxy_server,
      net::HttpAuthController* http_auth_controller,
      bool tunnel,
      bool using_spdy,
      net::NextProto negotiated_protocol,
      net::ProxyDelegate* proxy_delegate,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    ADD_FAILURE() << "Called CreateProxyClientSocket()";
    return nullptr;
  }

 private:
  std::vector<std::unique_ptr<MockUDPSocket>> udp_sockets_;

  DISALLOW_COPY_AND_ASSIGN(MockSocketFactory);
};

// Tests myIpAddress() when there is a route to 8.8.8.8.
TEST(PacLibraryTest, PacMyIpAddress8888) {
  MockSocketFactory factory;
  factory.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1");

  auto result = PacMyIpAddressForTest(&factory, {});
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("192.168.1.1", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, but there is one to
// 2001:4860:4860::8888.
TEST(PacLibraryTest, PacMyIpAddress2001) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectSuccess("2001:4860:4860::8888", "2001::beef");

  net::AddressList dns_result;

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("2001::beef", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, however getaddrinfo(gethostname()) finds results. Most
// of those results are skipped over, and the IPv4 one is favored.
TEST(PacLibraryTest, PacMyIpAddressHostname) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result = CreateAddressList({
      "169.254.13.16",
      "127.0.0.1",
      "::1",
      "fe89::beef",
      "2001::f001",
      "178.1.99.3",
      "192.168.1.3",
  });

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("178.1.99.3", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, however getaddrinfo(gethostname()) finds multiple IPv6
// results.
TEST(PacLibraryTest, PacMyIpAddressHostnameAllIPv6) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result =
      CreateAddressList({"::1", "2001::f001", "2001::f00d", "169.254.0.6"});

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("2001::f001", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, no acceptable result in getaddrinfo(gethostname()),
// however there is a route for private address.
TEST(PacLibraryTest, PacMyIpAddressPrivateIPv4) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result = CreateAddressList({
      "169.254.13.16",
      "127.0.0.1",
      "::1",
      "fe89::beef",
  });

  factory.AddUDPConnectSuccess("10.0.0.0", "127.0.0.1");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectSuccess("192.168.0.0", "63.31.9.8");

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("63.31.9.8", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, no acceptable result in getaddrinfo(gethostname()),
// however there is a route for private address.
TEST(PacLibraryTest, PacMyIpAddressPrivateIPv6) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result;

  factory.AddUDPConnectSuccess("10.0.0.0", "127.0.0.1");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectSuccess("FC00::", "2001::7777");

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("2001::7777", result.front().ToString());
}

// Tests myIpAddress() when there are no routes, and getaddrinfo(gethostname())
// fails.
TEST(PacLibraryTest, PacMyIpAddressAllFail) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result;

  factory.AddUDPConnectFailure("10.0.0.0");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  EXPECT_EQ(0u, result.size());
}

// Tests myIpAddress() when there are no routes, and
// getaddrinfo(gethostname()) only returns loopback.
TEST(PacLibraryTest, PacMyIpAddressAllFailOrLoopback) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result = CreateAddressList({"127.0.0.1", "::1"});

  factory.AddUDPConnectFailure("10.0.0.0");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  EXPECT_EQ(0u, result.size());
}

// Tests myIpAddress() when there is only an IPv6 link-local address.
TEST(PacLibraryTest, PacMyIpAddressAllFailHasLinkLocal) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result =
      CreateAddressList({"127.0.0.1", "::1", "fe81::8881"});

  factory.AddUDPConnectFailure("10.0.0.0");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("fe81::8881", result.front().ToString());
}

// Tests myIpAddress() when there are only link-local addresses. The IPv4
// link-local address is favored.
TEST(PacLibraryTest, PacMyIpAddressAllFailHasLinkLocalFavorIPv4) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result =
      CreateAddressList({"127.0.0.1", "::1", "fe81::8881", "169.254.89.133"});

  factory.AddUDPConnectFailure("10.0.0.0");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("169.254.89.133", result.front().ToString());
}

// Tests myIpAddressEx() when there is a route to 8.8.8.8 but not one to
// 2001:4860:4860::8888
TEST(PacLibraryTest, PacMyIpAddressEx8888) {
  MockSocketFactory factory;
  factory.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  auto result = PacMyIpAddressExForTest(&factory, {});
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("192.168.1.1", result.front().ToString());
}

// Tests myIpAddressEx() when there is a route to 2001:4860:4860::8888 but
// not 8.8.8.8.
TEST(PacLibraryTest, PacMyIpAddressEx2001) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectSuccess("2001:4860:4860::8888", "2001::3333");

  net::AddressList dns_result;

  auto result = PacMyIpAddressExForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("2001::3333", result.front().ToString());
}

// Tests myIpAddressEx() when there is a route to both 8.8.8.8 and
// 2001:4860:4860::8888.
TEST(PacLibraryTest, PacMyIpAddressEx8888And2001) {
  MockSocketFactory factory;
  factory.AddUDPConnectSuccess("8.8.8.8", "192.168.17.8");
  factory.AddUDPConnectSuccess("2001:4860:4860::8888", "2001::8333");

  net::AddressList dns_result;

  auto result = PacMyIpAddressExForTest(&factory, dns_result);
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("192.168.17.8", result.front().ToString());
  EXPECT_EQ("2001::8333", result.back().ToString());
}

// Tests myIpAddressEx() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, however getaddrinfo(gethostname()) finds results. Some
// of those results are skipped due to being link-local and loopback.
TEST(PacLibraryTest, PacMyIpAddressExHostname) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result = CreateAddressList({
      "169.254.13.16",
      "::1",
      "fe89::beef",
      "2001::bebe",
      "178.1.99.3",
      "127.0.0.1",
      "192.168.1.3",
  });

  auto result = PacMyIpAddressExForTest(&factory, dns_result);
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("2001::bebe", result[0].ToString());
  EXPECT_EQ("178.1.99.3", result[1].ToString());
  EXPECT_EQ("192.168.1.3", result[2].ToString());
}

// Tests myIpAddressEx() when routes are found for private IP space.
TEST(PacLibraryTest, PacMyIpAddressExPrivateDuplicates) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result;

  factory.AddUDPConnectSuccess("10.0.0.0", "192.168.3.3");
  factory.AddUDPConnectSuccess("172.16.0.0", "192.168.3.4");
  factory.AddUDPConnectSuccess("192.168.0.0", "192.168.3.3");
  factory.AddUDPConnectSuccess("FC00::", "2001::beef");

  auto result = PacMyIpAddressExForTest(&factory, dns_result);

  // Note that 192.168.3.3. was probed twice, but only added once to the final
  // result.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("192.168.3.3", result[0].ToString());
  EXPECT_EQ("192.168.3.4", result[1].ToString());
  EXPECT_EQ("2001::beef", result[2].ToString());
}

// Tests myIpAddressEx() when there are no routes, and
// getaddrinfo(gethostname()) fails.
TEST(PacLibraryTest, PacMyIpAddressExAllFail) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result;

  factory.AddUDPConnectFailure("10.0.0.0");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressExForTest(&factory, dns_result);
  EXPECT_EQ(0u, result.size());
}

// Tests myIpAddressEx() when there are only IPv6 link-local address.
TEST(PacLibraryTest, PacMyIpAddressExAllFailHasLinkLocal) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result =
      CreateAddressList({"127.0.0.1", "::1", "fe81::8881", "fe80::8899"});

  factory.AddUDPConnectFailure("10.0.0.0");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectSuccess("FC00::", "fe80::1");

  auto result = PacMyIpAddressExForTest(&factory, dns_result);
  // There were four link-local addresses found, but only the first one is
  // returned.
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("fe81::8881", result.front().ToString());
}

// Tests myIpAddressEx() when there are only link-local addresses. The IPv4
// link-local address is favored.
TEST(PacLibraryTest, PacMyIpAddressExAllFailHasLinkLocalFavorIPv4) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result =
      CreateAddressList({"127.0.0.1", "::1", "fe81::8881", "169.254.89.133"});

  factory.AddUDPConnectFailure("10.0.0.0");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressExForTest(&factory, dns_result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("169.254.89.133", result.front().ToString());
}

// Tests myIpAddressEx() when there are no routes, and
// getaddrinfo(gethostname()) only returns loopback.
TEST(PacLibraryTest, PacMyIpAddressExAllFailOrLoopback) {
  MockSocketFactory factory;
  factory.AddUDPConnectFailure("8.8.8.8");
  factory.AddUDPConnectFailure("2001:4860:4860::8888");

  net::AddressList dns_result = CreateAddressList({"127.0.0.1", "::1"});

  factory.AddUDPConnectFailure("10.0.0.0");
  factory.AddUDPConnectFailure("172.16.0.0");
  factory.AddUDPConnectFailure("192.168.0.0");
  factory.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressExForTest(&factory, dns_result);
  EXPECT_EQ(0u, result.size());
}

}  // namespace
}  // namespace network
