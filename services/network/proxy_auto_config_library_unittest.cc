// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_auto_config_library.h"

#include <algorithm>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class MockClient : public proxy_resolver::mojom::HostResolverRequestClient {
 public:
  MockClient(
      mojo::PendingReceiver<proxy_resolver::mojom::HostResolverRequestClient>
          pending_receiver,
      base::OnceClosure results_callback)
      : receiver_(this, std::move(pending_receiver)),
        results_callback_(std::move(results_callback)) {}

  void ReportResult(int error, const net::IPAddressList& results) override {
    results_ = results;
    std::move(results_callback_).Run();
  }

  net::IPAddressList GetResults() const { return results_; }

 private:
  mojo::Receiver<proxy_resolver::mojom::HostResolverRequestClient> receiver_;
  base::OnceClosure results_callback_;
  net::IPAddressList results_;
};

// Helper for verifying whether the address list returned by myIpAddress() /
// myIpAddressEx() looks correct.
void VerifyActualMyIpAddresses(const net::IPAddressList& test_list) {
  // Enumerate all of the IP addresses for the system (skipping loopback and
  // link-local ones). This is used as a reference implementation to check
  // whether `test_list` (which was obtained using a different strategy) looks
  // correct.
  std::set<net::IPAddress> candidates;
  net::NetworkInterfaceList networks;
  net::GetNetworkList(&networks, net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES);
  for (const auto& network : networks) {
    if (network.address.IsLinkLocal() || network.address.IsLoopback())
      continue;
    candidates.insert(network.address);
  }

  EXPECT_GT(test_list.size(), 0u);

  // Ordinarily the machine running this test will have an IP address. However
  // for some bot configurations (notably Android) that may not be the case.
  if (candidates.empty()) {
    // There are no possible candidates, so myIpAddress() should return IPV4
    // localhost.
    ASSERT_EQ(1u, test_list.size());
    EXPECT_EQ("127.0.0.1", test_list.front().ToString());
    return;
  }

  // `test_list` should be a subset of `candidates`.
  for (const auto& ip : test_list)
    EXPECT_EQ(1u, candidates.count(ip));
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

  MockUDPSocket(const MockUDPSocket&) = delete;
  MockUDPSocket& operator=(const MockUDPSocket&) = delete;

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
  int ConnectUsingNetwork(net::handles::NetworkHandle network,
                          const net::IPEndPoint& address) override {
    ADD_FAILURE() << "Called ConnectUsingNetwork()";
    return net::ERR_UNEXPECTED;
  }
  int ConnectUsingDefaultNetwork(const net::IPEndPoint& address) override {
    ADD_FAILURE() << "Called ConnectUsingDefaultNetwork()";
    return net::ERR_UNEXPECTED;
  }
  int ConnectAsync(const net::IPEndPoint& address,
                   net::CompletionOnceCallback callback) override {
    ADD_FAILURE() << "Called ConnectAsync()";
    return net::ERR_UNEXPECTED;
  }
  int ConnectUsingNetworkAsync(net::handles::NetworkHandle network,
                               const net::IPEndPoint& address,
                               net::CompletionOnceCallback callback) override {
    ADD_FAILURE() << "Called ConnectUsingNetworkAsync()";
    return net::ERR_UNEXPECTED;
  }
  int ConnectUsingDefaultNetworkAsync(
      const net::IPEndPoint& address,
      net::CompletionOnceCallback callback) override {
    ADD_FAILURE() << "Called ConnectUsingDefaultNetworkAsync()";
    return net::ERR_UNEXPECTED;
  }
  net::handles::NetworkHandle GetBoundNetwork() const override {
    ADD_FAILURE() << "Called GetBoundNetwork()";
    return network_;
  }
  void ApplySocketTag(const net::SocketTag& tag) override {
    ADD_FAILURE() << "Called ApplySocketTag()";
  }
  int SetMulticastInterface(uint32_t interface_index) override {
    ADD_FAILURE() << "Called SetMulticastInterface()";
    return net::ERR_UNEXPECTED;
  }

 private:
  net::NetLogWithSource net_log_;
  net::handles::NetworkHandle network_;

  net::IPAddress peer_ip_;
  net::IPAddress local_ip_;
  net::Error connect_error_;
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

  MockSocketFactory(const MockSocketFactory&) = delete;
  MockSocketFactory& operator=(const MockSocketFactory&) = delete;

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
      net::NetworkQualityEstimator* network_quality_estimator,
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

 private:
  std::vector<std::unique_ptr<MockUDPSocket>> udp_sockets_;
};

class PacLibraryTest : public testing::Test {
 public:
  PacLibraryTest() = default;
  ~PacLibraryTest() override = default;

 protected:
  net::IPAddressList PacMyIpAddressForTest() {
    impl_ = base::MakeRefCounted<MyIpAddressImpl>(
        MyIpAddressImpl::Mode::kMyIpAddress);
    return RunMyIpAddressUntilCompletion();
  }

  net::IPAddressList PacMyIpAddressExForTest() {
    impl_ = base::MakeRefCounted<MyIpAddressImpl>(
        MyIpAddressImpl::Mode::kMyIpAddressEx);
    return RunMyIpAddressUntilCompletion();
  }

  net::IPAddressList RunMyIpAddressUntilCompletion() {
    if (use_mocks_) {
      impl_->SetSocketFactoryForTest(&factory_);
      impl_->SetDNSResultForTest(dns_result_);
    }

    base::RunLoop run_loop;
    mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
        remote;
    MockClient client(remote.InitWithNewPipeAndPassReceiver(),
                      run_loop.QuitClosure());
    impl_->AddRequest(std::move(remote));
    run_loop.Run();
    return client.GetResults();
  }

  void SetRealTest() { use_mocks_ = false; }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MyIpAddressImpl> impl_;
  MockSocketFactory factory_;
  net::AddressList dns_result_;
  bool use_mocks_ = true;
};

// Tests for actual PacMyIpAddress() and PacMyIpAddressEx() (real socket
// connections and DNS results rather than mocks)
TEST_F(PacLibraryTest, ActualPacMyIpAddress) {
  SetRealTest();
  auto my_ip_addresses = PacMyIpAddressForTest();

  VerifyActualMyIpAddresses(my_ip_addresses);
}

TEST_F(PacLibraryTest, ActualPacMyIpAddressEx) {
  SetRealTest();
  auto my_ip_addresses = PacMyIpAddressExForTest();

  VerifyActualMyIpAddresses(my_ip_addresses);
}

// Tests myIpAddress() when there is a route to 8.8.8.8.
TEST_F(PacLibraryTest, PacMyIpAddress8888) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1");

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("192.168.1.1", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, but there is one to
// 2001:4860:4860::8888.
TEST_F(PacLibraryTest, PacMyIpAddress2001) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2001::beef");

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("2001::beef", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, however getaddrinfo(gethostname()) finds results. Most
// of those results are skipped over, and the IPv4 one is favored.
TEST_F(PacLibraryTest, PacMyIpAddressHostname) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ = CreateAddressList({
      "169.254.13.16",
      "127.0.0.1",
      "::1",
      "fe89::beef",
      "2001::f001",
      "178.1.99.3",
      "192.168.1.3",
  });

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("178.1.99.3", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, however getaddrinfo(gethostname()) finds multiple IPv6
// results.
TEST_F(PacLibraryTest, PacMyIpAddressHostnameAllIPv6) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ =
      CreateAddressList({"::1", "2001::f001", "2001::f00d", "169.254.0.6"});

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("2001::f001", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, no acceptable result in getaddrinfo(gethostname()),
// however there is a route for private address.
TEST_F(PacLibraryTest, PacMyIpAddressPrivateIPv4) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ = CreateAddressList({
      "169.254.13.16",
      "127.0.0.1",
      "::1",
      "fe89::beef",
  });

  factory_.AddUDPConnectSuccess("10.0.0.0", "127.0.0.1");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectSuccess("192.168.0.0", "63.31.9.8");

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("63.31.9.8", result.front().ToString());
}

// Tests myIpAddress() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, no acceptable result in getaddrinfo(gethostname()),
// however there is a route for private address.
TEST_F(PacLibraryTest, PacMyIpAddressPrivateIPv6) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  // No DNS result

  factory_.AddUDPConnectSuccess("10.0.0.0", "127.0.0.1");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectSuccess("FC00::", "2001::7777");

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("2001::7777", result.front().ToString());
}

// Tests myIpAddress() when there are no routes, and getaddrinfo(gethostname())
// fails.
TEST_F(PacLibraryTest, PacMyIpAddressAllFail) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  // No DNS result

  factory_.AddUDPConnectFailure("10.0.0.0");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressForTest();
  // Every method failed, so myIpAddress() should return IPV4 localhost.
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("127.0.0.1", result.front().ToString());
}

// Tests myIpAddress() when there are no routes, and
// getaddrinfo(gethostname()) only returns loopback.
TEST_F(PacLibraryTest, PacMyIpAddressAllFailOrLoopback) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ = CreateAddressList({"127.0.0.1", "::1"});

  factory_.AddUDPConnectFailure("10.0.0.0");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressForTest();
  // Every method failed, so myIpAddress() should return IPV4 localhost.
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("127.0.0.1", result.front().ToString());
}

// Tests myIpAddress() when there is only an IPv6 link-local address.
TEST_F(PacLibraryTest, PacMyIpAddressAllFailHasLinkLocal) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ = CreateAddressList({"127.0.0.1", "::1", "fe81::8881"});

  factory_.AddUDPConnectFailure("10.0.0.0");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("fe81::8881", result.front().ToString());
}

// Tests myIpAddress() when there are only link-local addresses. The IPv4
// link-local address is favored.
TEST_F(PacLibraryTest, PacMyIpAddressAllFailHasLinkLocalFavorIPv4) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ =
      CreateAddressList({"127.0.0.1", "::1", "fe81::8881", "169.254.89.133"});

  factory_.AddUDPConnectFailure("10.0.0.0");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("169.254.89.133", result.front().ToString());
}

// Tests myIpAddressEx() when there is a route to 8.8.8.8 but not one to
// 2001:4860:4860::8888
TEST_F(PacLibraryTest, PacMyIpAddressEx8888) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  auto result = PacMyIpAddressExForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("192.168.1.1", result.front().ToString());
}

// Tests myIpAddressEx() when there is a route to 2001:4860:4860::8888 but
// not 8.8.8.8.
TEST_F(PacLibraryTest, PacMyIpAddressEx2001) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2001::3333");

  auto result = PacMyIpAddressExForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("2001::3333", result.front().ToString());
}

// Tests myIpAddressEx() when there is a route to both 8.8.8.8 and
// 2001:4860:4860::8888.
TEST_F(PacLibraryTest, PacMyIpAddressEx8888And2001) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.17.8");
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2001::8333");

  auto result = PacMyIpAddressExForTest();
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("192.168.17.8", result.front().ToString());
  EXPECT_EQ("2001::8333", result.back().ToString());
}

// Tests myIpAddressEx() when there is no route to 8.8.8.8, no route to
// 2001:4860:4860::8888, however getaddrinfo(gethostname()) finds results. Some
// of those results are skipped due to being link-local and loopback.
TEST_F(PacLibraryTest, PacMyIpAddressExHostname) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ = CreateAddressList({
      "169.254.13.16",
      "::1",
      "fe89::beef",
      "2001::bebe",
      "178.1.99.3",
      "127.0.0.1",
      "192.168.1.3",
  });

  auto result = PacMyIpAddressExForTest();
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("2001::bebe", result[0].ToString());
  EXPECT_EQ("178.1.99.3", result[1].ToString());
  EXPECT_EQ("192.168.1.3", result[2].ToString());
}

// Tests myIpAddressEx() when routes are found for private IP space.
TEST_F(PacLibraryTest, PacMyIpAddressExPrivateDuplicates) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  // No DNS result

  factory_.AddUDPConnectSuccess("10.0.0.0", "192.168.3.3");
  factory_.AddUDPConnectSuccess("172.16.0.0", "192.168.3.4");
  factory_.AddUDPConnectSuccess("192.168.0.0", "192.168.3.3");
  factory_.AddUDPConnectSuccess("FC00::", "2001::beef");

  auto result = PacMyIpAddressExForTest();

  // Note that 192.168.3.3. was probed twice, but only added once to the final
  // result.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("192.168.3.3", result[0].ToString());
  EXPECT_EQ("192.168.3.4", result[1].ToString());
  EXPECT_EQ("2001::beef", result[2].ToString());
}

// Tests myIpAddressEx() when there are no routes, and
// getaddrinfo(gethostname()) fails.
TEST_F(PacLibraryTest, PacMyIpAddressExAllFail) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  // No DNS result

  factory_.AddUDPConnectFailure("10.0.0.0");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressExForTest();
  // Every method failed, so myIpAddress() should return IPV4 localhost.
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("127.0.0.1", result.front().ToString());
}

// Tests myIpAddressEx() when there are only IPv6 link-local address.
TEST_F(PacLibraryTest, PacMyIpAddressExAllFailHasLinkLocal) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ =
      CreateAddressList({"127.0.0.1", "::1", "fe81::8881", "fe80::8899"});

  factory_.AddUDPConnectFailure("10.0.0.0");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectSuccess("FC00::", "fe80::1");

  auto result = PacMyIpAddressExForTest();
  // There were four link-local addresses found, but only the first one is
  // returned.
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("fe81::8881", result.front().ToString());
}

// Tests myIpAddressEx() when there are only link-local addresses. The IPv4
// link-local address is favored.
TEST_F(PacLibraryTest, PacMyIpAddressExAllFailHasLinkLocalFavorIPv4) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ =
      CreateAddressList({"127.0.0.1", "::1", "fe81::8881", "169.254.89.133"});

  factory_.AddUDPConnectFailure("10.0.0.0");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressExForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("169.254.89.133", result.front().ToString());
}

// Tests myIpAddressEx() when there are no routes, and
// getaddrinfo(gethostname()) only returns loopback.
TEST_F(PacLibraryTest, PacMyIpAddressExAllFailOrLoopback) {
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ = CreateAddressList({"127.0.0.1", "::1"});

  factory_.AddUDPConnectFailure("10.0.0.0");
  factory_.AddUDPConnectFailure("172.16.0.0");
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::");

  auto result = PacMyIpAddressExForTest();
  // Every method failed, so myIpAddress() should return IPV4 localhost.
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("127.0.0.1", result.front().ToString());
}

TEST_F(PacLibraryTest, PacMyIpAddressExRunMultipleTimes) {
  impl_ = base::MakeRefCounted<MyIpAddressImpl>(
      MyIpAddressImpl::Mode::kMyIpAddressEx);

  // Run the PacMyIpAddressExHostname test.
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_ = CreateAddressList({
      "169.254.13.16",
      "::1",
      "fe89::beef",
      "2001::bebe",
      "178.1.99.3",
      "127.0.0.1",
      "192.168.1.3",
  });

  auto result = RunMyIpAddressUntilCompletion();
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("2001::bebe", result[0].ToString());
  EXPECT_EQ("178.1.99.3", result[1].ToString());
  EXPECT_EQ("192.168.1.3", result[2].ToString());

  // Run the PacMyIpAddressExPrivateDuplicates with the same `impl_` as the
  // previous test.
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  dns_result_.clear();

  factory_.AddUDPConnectSuccess("10.0.0.0", "192.168.3.3");
  factory_.AddUDPConnectSuccess("172.16.0.0", "192.168.3.4");
  factory_.AddUDPConnectSuccess("192.168.0.0", "192.168.3.3");
  factory_.AddUDPConnectSuccess("FC00::", "2001::beef");

  result = RunMyIpAddressUntilCompletion();

  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("192.168.3.3", result[0].ToString());
  EXPECT_EQ("192.168.3.4", result[1].ToString());
  EXPECT_EQ("2001::beef", result[2].ToString());
}

}  // namespace
}  // namespace network
