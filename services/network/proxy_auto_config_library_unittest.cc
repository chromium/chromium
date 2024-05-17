// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_auto_config_library.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/dns/host_resolver_system_task.h"
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

net::IPAddress CreateIPAddress(std::string_view literal) {
  net::IPAddress result;
  if (!result.AssignFromIPLiteral(literal)) {
    ADD_FAILURE() << "Failed parsing IP: " << literal;
    return net::IPAddress();
  }
  return result;
}

class MockHostResolverProc : public net::HostResolverProc {
 public:
  MockHostResolverProc() : HostResolverProc(nullptr) {}

  void SetDnsResult(const std::vector<std::string_view>& ip_literals) {
    result_.clear();
    for (const auto& ip : ip_literals)
      result_.push_back(net::IPEndPoint(CreateIPAddress(ip), 8080));
  }

  int Resolve(const std::string& hostname,
              net::AddressFamily address_family,
              net::HostResolverFlags host_resolver_flags,
              net::AddressList* addrlist,
              int* os_error) override {
    EXPECT_EQ(hostname, net::GetHostName());
    *addrlist = result_;
    return net::OK;
  }

 private:
  ~MockHostResolverProc() override = default;

  net::AddressList result_;
};

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
  int SetRecvTos() override {
    ADD_FAILURE() << "Called SetRecvTos()";
    return net::ERR_UNEXPECTED;
  }
  int SetTos(net::DiffServCodePoint dscp, net::EcnCodePoint ecn) override {
    ADD_FAILURE() << "Called SetTos()";
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
    EXPECT_FALSE(connect_async_);
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
    if (peer_ip_.IsValid()) {
      EXPECT_EQ(peer_ip_.ToString(), address.address().ToString());
    }
    if (connect_async_) {
      if (connect_callback_) {
        *connect_callback_ =
            base::BindOnce(std::move(callback), connect_error_);
        connect_callback_ = nullptr;
      }
      return net::ERR_IO_PENDING;
    }
    return connect_error_;
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

  // When ConnectAsync() is called, it should return ERR_IO_PENDING and store
  // the callback in `*connect_callback_`. This callback can be run later by
  // test code by calling `*connect_callback_` (by running
  // MockSocketFactory::RunAsyncConnectCallbacks()).
  void SetAsyncConnect(base::OnceClosure* connect_callback) {
    connect_async_ = true;
    connect_callback_ = connect_callback;
  }

  net::DscpAndEcn GetLastTos() const override {
    ADD_FAILURE() << "Called GetLastTos()";
    return {net::DSCP_DEFAULT, net::ECN_DEFAULT};
  }

 private:
  net::NetLogWithSource net_log_;
  net::handles::NetworkHandle network_;

  net::IPAddress peer_ip_;
  net::IPAddress local_ip_;
  net::Error connect_error_;
  bool connect_async_ = false;
  raw_ptr<base::OnceClosure> connect_callback_;
};

class MockSocketFactory : public net::ClientSocketFactory {
 public:
  MockSocketFactory() = default;

  // Connect successes and failures that complete asynchronously
  void AddUDPConnectSuccess(std::string_view peer_ip_literal,
                            std::string_view local_ip_literal,
                            int connect_order = -1) {
    auto peer_ip = CreateIPAddress(peer_ip_literal);
    auto local_ip = CreateIPAddress(local_ip_literal);

    // The address family of local and peer IP must match.
    ASSERT_EQ(peer_ip.size(), local_ip.size());

    AddUDPConnectResult(std::move(peer_ip), std::move(local_ip), net::OK,
                        connect_order);
  }

  void AddUDPConnectFailure(std::string_view peer_ip, int connect_order = -1) {
    AddUDPConnectResult(CreateIPAddress(peer_ip), net::IPAddress(),
                        net::ERR_ADDRESS_UNREACHABLE, connect_order);
  }

  MockSocketFactory(const MockSocketFactory&) = delete;
  MockSocketFactory& operator=(const MockSocketFactory&) = delete;

  ~MockSocketFactory() override {
    if (must_use_all_sockets_) {
      EXPECT_EQ(0u, udp_sockets_.size())
          << "Not all of the mock sockets were consumed.";
    }
  }

  // net::ClientSocketFactory
  std::unique_ptr<net::DatagramClientSocket> CreateDatagramClientSocket(
      net::DatagramSocket::BindType bind_type,
      net::NetLog* net_log,
      const net::NetLogSource& source) override {
    if (udp_sockets_.empty()) {
      // If we don't have a result for this one, return a socket that never
      // connects (because it is set to connect aysnchronously and isn't added
      // to `udp_socket_ptrs`).
      // It should be deleted when MyIpAddressImpl is deleted.
      auto socket = std::make_unique<MockUDPSocket>(
          net::IPAddress(), net::IPAddress(), net::ERR_IO_PENDING);
      socket->SetAsyncConnect(nullptr);
      return socket;
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

  // Used to test async-connected sockets. Returns false if we are not finished
  // running callbacks.
  bool RunAsyncConnectCallbacks() {
    while (!connect_callbacks_.empty()) {
      base::OnceClosure& first_callback = connect_callbacks_.front();
      if (!first_callback)
        return false;
      std::move(first_callback).Run();
      connect_callbacks_.pop_front();
    }
    return true;
  }

  void SetCanLeaveSocketsUnused() { must_use_all_sockets_ = false; }

 private:
  void AddUDPConnectResult(net::IPAddress peer_ip,
                           net::IPAddress local_ip,
                           net::Error net_error,
                           int connect_order) {
    auto socket = std::make_unique<MockUDPSocket>(
        std::move(peer_ip), std::move(local_ip), net_error);
    if (connect_order >= 0) {
      connect_callbacks_.resize(
          std::max(size_t(connect_order + 1), connect_callbacks_.size()));
      CHECK(!connect_callbacks_[connect_order]);
      socket->SetAsyncConnect(&connect_callbacks_[connect_order]);
    }
    udp_sockets_.push_back(std::move(socket));
  }

  // Connection callbacks for the sockets, in order of async connection
  // completion. Entries in `udp_sockets_` may point to these.
  std::deque<base::OnceClosure> connect_callbacks_;
  std::vector<std::unique_ptr<MockUDPSocket>> udp_sockets_;
  // Unit tests should always consume all of the mock UDP sockets unless this is
  // set to false.
  bool must_use_all_sockets_ = true;
};

class PacLibraryTest : public testing::Test {
 public:
  PacLibraryTest()
      : host_resolver_proc_(base::MakeRefCounted<MockHostResolverProc>()) {
    net::EnsureSystemHostResolverCallReady();
  }
  ~PacLibraryTest() override = default;

 protected:
  net::IPAddressList PacMyIpAddressForTest() {
    impl_ =
        std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddress);
    return RunMyIpAddressUntilCompletion();
  }

  net::IPAddressList PacMyIpAddressExForTest() {
    impl_ = std::make_unique<MyIpAddressImpl>(
        MyIpAddressImpl::Mode::kMyIpAddressEx);
    return RunMyIpAddressUntilCompletion();
  }

  net::IPAddressList RunMyIpAddressUntilCompletion() {
    RunAsyncConnectCallbacksAndPostAgain();

    if (use_mocks_) {
      impl_->SetSocketFactoryForTest(&factory_);
      impl_->SetHostResolverProcForTest(host_resolver_proc_);
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

  void RunAsyncConnectCallbacksAndPostAgain() {
    bool finished = factory_.RunAsyncConnectCallbacks();
    // If all the ConnectAsync() completion callbacks haven't been called yet
    // they may need to in the future.
    if (!finished) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&PacLibraryTest::RunAsyncConnectCallbacksAndPostAgain,
                         base::Unretained(this)));
    }
  }

  void SetRealTest() { use_mocks_ = false; }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MyIpAddressImpl> impl_;
  MockSocketFactory factory_;
  scoped_refptr<MockHostResolverProc> host_resolver_proc_;
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

TEST_F(PacLibraryTest, PacMyIpAddress8888AsyncConnect) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1", 0);

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("192.168.1.1", result.front().ToString());
}

// Tests successful async-completion of the connections.
TEST_F(PacLibraryTest, PacMyIpAddress8888AsyncConnect2) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1", 0);
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2002::beef", 1);

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("192.168.1.1", result.front().ToString());
}

TEST_F(PacLibraryTest, PacMyIpAddress8888AsyncConnect2OutOfOrder) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1", 1);
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2002::beef", 0);

  auto result = PacMyIpAddressForTest();
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("192.168.1.1", result.front().ToString());
}

TEST_F(PacLibraryTest, PacMyIpAddress8888AsyncConnect2OutOfOrder2) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1", 0);
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2002::beef");

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

  host_resolver_proc_->SetDnsResult({
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

  host_resolver_proc_->SetDnsResult(
      {"::1", "2001::f001", "2001::f00d", "169.254.0.6"});

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

  host_resolver_proc_->SetDnsResult({
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

  host_resolver_proc_->SetDnsResult({"127.0.0.1", "::1"});

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

  host_resolver_proc_->SetDnsResult({"127.0.0.1", "::1", "fe81::8881"});

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

  host_resolver_proc_->SetDnsResult(
      {"127.0.0.1", "::1", "fe81::8881", "169.254.89.133"});

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

  host_resolver_proc_->SetDnsResult({
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

// Tests the same as above, but some of the connections complete asynchronously.
TEST_F(PacLibraryTest, PacMyIpAddressExPrivateDuplicatesAsyncConnect) {
  factory_.AddUDPConnectFailure("8.8.8.8", 1);
  factory_.AddUDPConnectFailure("2001:4860:4860::8888", 0);

  // No DNS result

  factory_.AddUDPConnectSuccess("10.0.0.0", "192.168.3.3", 4);
  factory_.AddUDPConnectSuccess("172.16.0.0", "192.168.3.4", 2);
  factory_.AddUDPConnectSuccess("192.168.0.0", "192.168.3.3", 5);
  factory_.AddUDPConnectSuccess("FC00::", "2001::beef", 3);

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

  host_resolver_proc_->SetDnsResult(
      {"127.0.0.1", "::1", "fe81::8881", "fe80::8899"});

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

  host_resolver_proc_->SetDnsResult(
      {"127.0.0.1", "::1", "fe81::8881", "169.254.89.133"});

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

  host_resolver_proc_->SetDnsResult({"127.0.0.1", "::1"});

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
  impl_ =
      std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddressEx);

  // Run the PacMyIpAddressExHostname test.
  factory_.AddUDPConnectFailure("8.8.8.8");
  factory_.AddUDPConnectFailure("2001:4860:4860::8888");

  host_resolver_proc_->SetDnsResult({
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

  host_resolver_proc_->SetDnsResult({});

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

TEST_F(PacLibraryTest, PacMyIpAddressExRunMultipleTimesAsync) {
  impl_ =
      std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddressEx);

  // Run the PacMyIpAddressExHostname test.
  factory_.AddUDPConnectFailure("8.8.8.8", 1);
  factory_.AddUDPConnectFailure("2001:4860:4860::8888", 0);

  host_resolver_proc_->SetDnsResult({
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

  // Results for the second test.
  factory_.AddUDPConnectFailure("8.8.8.8", 1);
  factory_.AddUDPConnectFailure("2001:4860:4860::8888", 0);

  host_resolver_proc_->SetDnsResult({});

  factory_.AddUDPConnectSuccess("10.0.0.0", "192.168.3.3", 2);
  factory_.AddUDPConnectSuccess("172.16.0.0", "192.168.3.4", 3);
  factory_.AddUDPConnectSuccess("192.168.0.0", "192.168.3.3", 4);
  factory_.AddUDPConnectSuccess("FC00::", "2001::beef", 5);

  // Run the PacMyIpAddressExPrivateDuplicates with the same `impl_` as the
  // previous test.
  result = RunMyIpAddressUntilCompletion();

  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("192.168.3.3", result[0].ToString());
  EXPECT_EQ("192.168.3.4", result[1].ToString());
  EXPECT_EQ("2001::beef", result[2].ToString());
}

// Same as above but the connections complete asynchronously.
TEST_F(PacLibraryTest, PacMyIpAddressExAllFailOrLoopbackAsyncConnect) {
  factory_.AddUDPConnectFailure("8.8.8.8", 0);
  factory_.AddUDPConnectFailure("2001:4860:4860::8888", 1);

  host_resolver_proc_->SetDnsResult({"127.0.0.1", "::1"});

  factory_.AddUDPConnectFailure("10.0.0.0", 2);
  factory_.AddUDPConnectFailure("172.16.0.0", 3);
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::", 4);

  auto result = PacMyIpAddressExForTest();
  // Every method failed, so myIpAddress() should return IPV4 localhost.
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("127.0.0.1", result.front().ToString());
}

// Tests that during async connect, MyIpAddressImpl can be deleted successfully.
TEST_F(PacLibraryTest, DeleteMyIpAddressImpl) {
  factory_.AddUDPConnectFailure("8.8.8.8", 1);
  factory_.AddUDPConnectFailure("2001:4860:4860::8888", 0);

  host_resolver_proc_->SetDnsResult({
      "169.254.13.16",
      "127.0.0.1",
      "::1",
      "fe89::beef",
  });

  factory_.AddUDPConnectSuccess("10.0.0.0", "127.0.0.1", 2);
  factory_.AddUDPConnectFailure("172.16.0.0", 3);
  factory_.AddUDPConnectSuccess("192.168.0.0", "63.31.9.8", 4);

  // The `impl_` doesn't actually use any of these sockets before it's deleted.
  factory_.SetCanLeaveSocketsUnused();

  impl_ =
      std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddress);
  impl_->SetSocketFactoryForTest(&factory_);
  impl_->SetHostResolverProcForTest(host_resolver_proc_);
  // Post a task that deletes `impl_`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { impl_.reset(); }));
  // Then post a task that runs the async connection callbacks.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&MockSocketFactory::RunAsyncConnectCallbacks),
          base::Unretained(&factory_)));
  // Now start the gathering.
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote;
  // NOTREACHED() because the request below should never complete.
  MockClient client(remote.InitWithNewPipeAndPassReceiver(),
                    base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }));
  impl_->AddRequest(std::move(remote));
  // Once all the tasks are run, `impl_` is guaranteed to be deleted.
  task_environment_.RunUntilIdle();
  CHECK(!impl_);
}

TEST_F(PacLibraryTest, ConnectMultipleRemotes) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1", 0);
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2002::beef", 1);

  impl_ =
      std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddress);
  impl_->SetSocketFactoryForTest(&factory_);
  impl_->SetHostResolverProcForTest(host_resolver_proc_);

  base::RunLoop run_loop;
  // Don't call the RunLoop's QuitClosure until both clients are done.
  base::RepeatingClosure results_cb =
      base::BarrierClosure(2, run_loop.QuitClosure());

  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote1;
  MockClient client1(remote1.InitWithNewPipeAndPassReceiver(), results_cb);
  impl_->AddRequest(std::move(remote1));

  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote2;
  MockClient client2(remote2.InitWithNewPipeAndPassReceiver(), results_cb);
  impl_->AddRequest(std::move(remote2));

  // Connections happen asynchronously so post a task to respond to connection
  // requests.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { factory_.RunAsyncConnectCallbacks(); }));

  // Runs until both clients have received results.
  run_loop.Run();

  net::IPAddressList result1 = client1.GetResults();
  ASSERT_EQ(1u, result1.size());
  EXPECT_EQ("192.168.1.1", result1.front().ToString());

  net::IPAddressList result2 = client2.GetResults();
  EXPECT_EQ(result1, result2);
}

// A clone of the above test that connects the second client after
// MyIpAddressImpl has lready started running.
TEST_F(PacLibraryTest, ConnectMultipleRemotesAsync) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1", 0);
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2002::beef", 1);

  impl_ =
      std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddress);
  impl_->SetSocketFactoryForTest(&factory_);
  impl_->SetHostResolverProcForTest(host_resolver_proc_);

  base::RunLoop run_loop;
  // Don't call the RunLoop's QuitClosure until both clients are done.
  base::RepeatingClosure results_cb =
      base::BarrierClosure(2, run_loop.QuitClosure());

  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote1;
  MockClient client1(remote1.InitWithNewPipeAndPassReceiver(), results_cb);

  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote2;
  MockClient client2(remote2.InitWithNewPipeAndPassReceiver(), results_cb);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { impl_->AddRequest(std::move(remote2)); }));

  // Connections happen asynchronously so post a task to respond to connection
  // requests.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { factory_.RunAsyncConnectCallbacks(); }));

  impl_->AddRequest(std::move(remote1));

  // Runs until both clients have received results.
  run_loop.Run();

  net::IPAddressList result1 = client1.GetResults();
  ASSERT_EQ(1u, result1.size());
  EXPECT_EQ("192.168.1.1", result1.front().ToString());

  net::IPAddressList result2 = client2.GetResults();
  EXPECT_EQ(result1, result2);
}

// Connect multiple remotes, but one disconnects.
TEST_F(PacLibraryTest, ConnectMultipleRemotesOneDisconnects) {
  factory_.AddUDPConnectSuccess("8.8.8.8", "192.168.1.1", 0);
  factory_.AddUDPConnectSuccess("2001:4860:4860::8888", "2002::beef", 1);

  impl_ =
      std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddress);
  impl_->SetSocketFactoryForTest(&factory_);
  impl_->SetHostResolverProcForTest(host_resolver_proc_);

  base::RunLoop run_loop;

  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote1;
  MockClient client1(remote1.InitWithNewPipeAndPassReceiver(),
                     run_loop.QuitClosure());

  // This second client will be disconnected as MyIpAddressImpl runs and should
  // never receive results.
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote2;
  std::unique_ptr<MockClient> client2 = std::make_unique<MockClient>(
      remote2.InitWithNewPipeAndPassReceiver(),
      base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }));

  // Post a task that deletes |client2|.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { client2.reset(); }));

  // Connections happen asynchronously so post a task to respond to connection
  // requests.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { factory_.RunAsyncConnectCallbacks(); }));

  impl_->AddRequest(std::move(remote1));
  impl_->AddRequest(std::move(remote2));

  run_loop.Run();

  net::IPAddressList result1 = client1.GetResults();
  ASSERT_EQ(1u, result1.size());
  EXPECT_EQ("192.168.1.1", result1.front().ToString());

  EXPECT_FALSE(client2);
}

// Connect multiple remotes, but one disconnects.
TEST_F(PacLibraryTest, ConnectMultipleRemotesButAllDisconnect) {
  factory_.AddUDPConnectFailure("8.8.8.8", 0);
  factory_.AddUDPConnectFailure("2001:4860:4860::8888", 1);

  // No DNS result.

  factory_.AddUDPConnectFailure("10.0.0.0", 2);
  factory_.AddUDPConnectFailure("172.16.0.0", 3);
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::", 4);

  // The last 4 sockets will not be used as all of the Remotes will disconnect
  // before that point.
  factory_.SetCanLeaveSocketsUnused();

  impl_ =
      std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddress);
  impl_->SetSocketFactoryForTest(&factory_);
  impl_->SetHostResolverProcForTest(host_resolver_proc_);

  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote1;
  std::unique_ptr<MockClient> client1 = std::make_unique<MockClient>(
      remote1.InitWithNewPipeAndPassReceiver(),
      base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }));

  // This second client will be disconnected as MyIpAddressImpl runs and should
  // never receive results.
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote2;
  std::unique_ptr<MockClient> client2 = std::make_unique<MockClient>(
      remote2.InitWithNewPipeAndPassReceiver(),
      base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }));

  // Post a task that deletes |client1|.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { client1.reset(); }));

  // Post a task to respond to connection requests.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { factory_.RunAsyncConnectCallbacks(); }));

  // Post a task that deletes |client2|.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { client2.reset(); }));

  impl_->AddRequest(std::move(remote1));
  impl_->AddRequest(std::move(remote2));

  // Can't reasonably use a RunLoop here because deleting the clients will post
  // disconnection callbacks and we want those to run.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(client1);
  EXPECT_FALSE(client2);
}

// Connect one remote, and during search, disconnect one remote but connect
// another.
TEST_F(PacLibraryTest, ConnectOneRemoteAndThenAnother) {
  factory_.AddUDPConnectFailure("8.8.8.8", 0);
  factory_.AddUDPConnectFailure("2001:4860:4860::8888", 1);

  // No DNS result.

  factory_.AddUDPConnectFailure("10.0.0.0", 2);
  factory_.AddUDPConnectFailure("172.16.0.0", 3);
  factory_.AddUDPConnectFailure("192.168.0.0");
  factory_.AddUDPConnectFailure("FC00::", 4);

  impl_ =
      std::make_unique<MyIpAddressImpl>(MyIpAddressImpl::Mode::kMyIpAddress);
  impl_->SetSocketFactoryForTest(&factory_);
  impl_->SetHostResolverProcForTest(host_resolver_proc_);

  base::RunLoop run_loop;

  // This client will be disconnected as MyIpAddressImpl runs and should never
  // receive results.
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote1;
  std::unique_ptr<MockClient> client1 = std::make_unique<MockClient>(
      remote1.InitWithNewPipeAndPassReceiver(),
      base::BindOnce([]() { NOTREACHED_IN_MIGRATION(); }));

  // This client will receive the results.
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient> remote2;
  std::unique_ptr<MockClient> client2 = std::make_unique<MockClient>(
      remote2.InitWithNewPipeAndPassReceiver(), run_loop.QuitClosure());

  // Post a task that deletes |client1| but connects |client2|.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        client1.reset();
        impl_->AddRequest(std::move(remote2));
      }));

  // Post a task to respond to connection requests.
  RunAsyncConnectCallbacksAndPostAgain();

  impl_->AddRequest(std::move(remote1));

  run_loop.Run();

  EXPECT_FALSE(client1);

  net::IPAddressList result2 = client2->GetResults();
  ASSERT_EQ(1u, result2.size());
  EXPECT_EQ("127.0.0.1", result2.front().ToString());
}

}  // namespace
}  // namespace network
