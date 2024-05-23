// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/address_sorter_posix.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// Used to map destination address to source address.
typedef std::map<IPAddress, IPAddress> AddressMapping;

IPAddress ParseIP(const std::string& str) {
  IPAddress addr;
  CHECK(addr.AssignFromIPLiteral(str));
  return addr;
}

// A mock socket which binds to source address according to AddressMapping.
class TestUDPClientSocket : public DatagramClientSocket {
 public:
  enum class ConnectMode { kSynchronous, kAsynchronous, kAsynchronousManual };
  explicit TestUDPClientSocket(const AddressMapping* mapping,
                               ConnectMode connect_mode)
      : mapping_(mapping), connect_mode_(connect_mode) {}

  TestUDPClientSocket(const TestUDPClientSocket&) = delete;
  TestUDPClientSocket& operator=(const TestUDPClientSocket&) = delete;

  ~TestUDPClientSocket() override = default;

  int Read(IOBuffer*, int, CompletionOnceCallback) override {
    NOTIMPLEMENTED();
    return OK;
  }
  int Write(IOBuffer*,
            int,
            CompletionOnceCallback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    NOTIMPLEMENTED();
    return OK;
  }
  int SetReceiveBufferSize(int32_t) override { return OK; }
  int SetSendBufferSize(int32_t) override { return OK; }
  int SetDoNotFragment() override { return OK; }
  int SetRecvTos() override { return OK; }
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override { return OK; }

  void Close() override {}
  int GetPeerAddress(IPEndPoint* address) const override {
    NOTIMPLEMENTED();
    return OK;
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    if (!connected_)
      return ERR_UNEXPECTED;
    *address = local_endpoint_;
    return OK;
  }
  void UseNonBlockingIO() override {}
  int SetMulticastInterface(uint32_t interface_index) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }

  int ConnectUsingNetwork(handles::NetworkHandle network,
                          const IPEndPoint& address) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }

  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }

  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override {
    DCHECK(callback);
    int rv = Connect(address);
    finish_connect_callback_ =
        base::BindOnce(&TestUDPClientSocket::RunConnectCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback), rv);
    if (connect_mode_ == ConnectMode::kAsynchronous) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(finish_connect_callback_));
      return ERR_IO_PENDING;
    } else if (connect_mode_ == ConnectMode::kAsynchronousManual) {
      return ERR_IO_PENDING;
    }
    return rv;
  }

  int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }

  int ConnectUsingDefaultNetworkAsync(
      const IPEndPoint& address,
      CompletionOnceCallback callback) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }

  handles::NetworkHandle GetBoundNetwork() const override {
    return handles::kInvalidNetworkHandle;
  }
  void ApplySocketTag(const SocketTag& tag) override {}
  void SetMsgConfirm(bool confirm) override {}

  int Connect(const IPEndPoint& remote) override {
    if (connected_)
      return ERR_UNEXPECTED;
    auto it = mapping_->find(remote.address());
    if (it == mapping_->end())
      return ERR_FAILED;
    connected_ = true;
    local_endpoint_ = IPEndPoint(it->second, 39874 /* arbitrary port */);
    return OK;
  }

  const NetLogWithSource& NetLog() const override { return net_log_; }

  void FinishConnect() { std::move(finish_connect_callback_).Run(); }

  DscpAndEcn GetLastTos() const override { return {DSCP_DEFAULT, ECN_DEFAULT}; }

 private:
  void RunConnectCallback(CompletionOnceCallback callback, int rv) {
    std::move(callback).Run(rv);
  }
  NetLogWithSource net_log_;
  raw_ptr<const AddressMapping> mapping_;
  bool connected_ = false;
  IPEndPoint local_endpoint_;
  ConnectMode connect_mode_;
  base::OnceClosure finish_connect_callback_;

  base::WeakPtrFactory<TestUDPClientSocket> weak_ptr_factory_{this};
};

// Creates TestUDPClientSockets and maintains an AddressMapping.
class TestSocketFactory : public ClientSocketFactory {
 public:
  TestSocketFactory() = default;

  TestSocketFactory(const TestSocketFactory&) = delete;
  TestSocketFactory& operator=(const TestSocketFactory&) = delete;

  ~TestSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType,
      NetLog*,
      const NetLogSource&) override {
    auto new_socket =
        std::make_unique<TestUDPClientSocket>(&mapping_, connect_mode_);
    if (socket_create_callback_) {
      socket_create_callback_.Run(new_socket.get());
    }
    return new_socket;
  }
  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList&,
      std::unique_ptr<SocketPerformanceWatcher>,
      net::NetworkQualityEstimator*,
      NetLog*,
      const NetLogSource&) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext*,
      std::unique_ptr<StreamSocket>,
      const HostPortPair&,
      const SSLConfig&) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  void AddMapping(const IPAddress& dst, const IPAddress& src) {
    mapping_[dst] = src;
  }
  void SetConnectMode(TestUDPClientSocket::ConnectMode connect_mode) {
    connect_mode_ = connect_mode;
  }
  void SetSocketCreateCallback(
      base::RepeatingCallback<void(TestUDPClientSocket*)>
          socket_create_callback) {
    socket_create_callback_ = std::move(socket_create_callback);
  }

 private:
  AddressMapping mapping_;
  TestUDPClientSocket::ConnectMode connect_mode_;
  base::RepeatingCallback<void(TestUDPClientSocket*)> socket_create_callback_;
};

void OnSortComplete(bool& completed,
                    std::vector<IPEndPoint>* sorted_buf,
                    CompletionOnceCallback callback,
                    bool success,
                    std::vector<IPEndPoint> sorted) {
  EXPECT_TRUE(success);
  completed = true;
  if (success)
    *sorted_buf = std::move(sorted);
  std::move(callback).Run(OK);
}

}  // namespace

// TaskEnvironment is required to register an IPAddressObserver from the
// constructor of AddressSorterPosix.
class AddressSorterPosixTest : public TestWithTaskEnvironment {
 protected:
  AddressSorterPosixTest()
      : sorter_(std::make_unique<AddressSorterPosix>(&socket_factory_)) {}

  void AddMapping(const std::string& dst, const std::string& src) {
    socket_factory_.AddMapping(ParseIP(dst), ParseIP(src));
  }

  void SetSocketCreateCallback(
      base::RepeatingCallback<void(TestUDPClientSocket*)>
          socket_create_callback) {
    socket_factory_.SetSocketCreateCallback(std::move(socket_create_callback));
  }

  void SetConnectMode(TestUDPClientSocket::ConnectMode connect_mode) {
    socket_factory_.SetConnectMode(connect_mode);
  }

  AddressSorterPosix::SourceAddressInfo* GetSourceInfo(
      const std::string& addr) {
    IPAddress address = ParseIP(addr);
    AddressSorterPosix::SourceAddressInfo* info =
        &sorter_->source_map_[address];
    if (info->scope == AddressSorterPosix::SCOPE_UNDEFINED)
      sorter_->FillPolicy(address, info);
    return info;
  }

  TestSocketFactory socket_factory_;
  std::unique_ptr<AddressSorterPosix> sorter_;
  bool completed_ = false;

 private:
  friend class AddressSorterPosixSyncOrAsyncTest;
};

// Parameterized subclass of AddressSorterPosixTest. Necessary because not every
// test needs to be parameterized.
class AddressSorterPosixSyncOrAsyncTest
    : public AddressSorterPosixTest,
      public testing::WithParamInterface<TestUDPClientSocket::ConnectMode> {
 protected:
  AddressSorterPosixSyncOrAsyncTest() { SetConnectMode(GetParam()); }

  // Verify that NULL-terminated |addresses| matches (-1)-terminated |order|
  // after sorting.
  void Verify(const char* const addresses[], const int order[]) {
    std::vector<IPEndPoint> endpoints;
    for (const char* const* addr = addresses; *addr != nullptr; ++addr)
      endpoints.emplace_back(ParseIP(*addr), 80);
    for (size_t i = 0; order[i] >= 0; ++i)
      CHECK_LT(order[i], static_cast<int>(endpoints.size()));

    std::vector<IPEndPoint> sorted;
    TestCompletionCallback callback;
    sorter_->Sort(endpoints,
                  base::BindOnce(&OnSortComplete, std::ref(completed_), &sorted,
                                 callback.callback()));
    callback.WaitForResult();

    for (size_t i = 0; (i < sorted.size()) || (order[i] >= 0); ++i) {
      IPEndPoint expected = order[i] >= 0 ? endpoints[order[i]] : IPEndPoint();
      IPEndPoint actual = i < sorted.size() ? sorted[i] : IPEndPoint();
      EXPECT_TRUE(expected == actual)
          << "Endpoint out of order at position " << i << "\n"
          << "  Actual: " << actual.ToString() << "\n"
          << "Expected: " << expected.ToString();
    }
    EXPECT_TRUE(completed_);
  }
};

INSTANTIATE_TEST_SUITE_P(
    AddressSorterPosix,
    AddressSorterPosixSyncOrAsyncTest,
    ::testing::Values(TestUDPClientSocket::ConnectMode::kSynchronous,
                      TestUDPClientSocket::ConnectMode::kAsynchronous));

// Rule 1: Avoid unusable destinations.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule1) {
  AddMapping("10.0.0.231", "10.0.0.1");
  const char* const addresses[] = {"::1", "10.0.0.231", "127.0.0.1", nullptr};
  const int order[] = { 1, -1 };
  Verify(addresses, order);
}

// Rule 2: Prefer matching scope.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule2) {
  AddMapping("3002::1", "4000::10");      // matching global
  AddMapping("ff32::1", "fe81::10");      // matching link-local
  AddMapping("fec1::1", "fec1::10");      // matching node-local
  AddMapping("3002::2", "::1");           // global vs. link-local
  AddMapping("fec1::2", "fe81::10");      // site-local vs. link-local
  AddMapping("8.0.0.1", "169.254.0.10");  // global vs. link-local
  // In all three cases, matching scope is preferred.
  const int order[] = { 1, 0, -1 };
  const char* const addresses1[] = {"3002::2", "3002::1", nullptr};
  Verify(addresses1, order);
  const char* const addresses2[] = {"fec1::2", "ff32::1", nullptr};
  Verify(addresses2, order);
  const char* const addresses3[] = {"8.0.0.1", "fec1::1", nullptr};
  Verify(addresses3, order);
}

// Rule 3: Avoid deprecated addresses.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule3) {
  // Matching scope.
  AddMapping("3002::1", "4000::10");
  GetSourceInfo("4000::10")->deprecated = true;
  AddMapping("3002::2", "4000::20");
  const char* const addresses[] = {"3002::1", "3002::2", nullptr};
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 4: Prefer home addresses.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule4) {
  AddMapping("3002::1", "4000::10");
  AddMapping("3002::2", "4000::20");
  GetSourceInfo("4000::20")->home = true;
  const char* const addresses[] = {"3002::1", "3002::2", nullptr};
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 5: Prefer matching label.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule5) {
  AddMapping("::1", "::1");                       // matching loopback
  AddMapping("::ffff:1234:1", "::ffff:1234:10");  // matching IPv4-mapped
  AddMapping("2001::1", "::ffff:1234:10");        // Teredo vs. IPv4-mapped
  AddMapping("2002::1", "2001::10");              // 6to4 vs. Teredo
  const int order[] = { 1, 0, -1 };
  {
    const char* const addresses[] = {"2001::1", "::1", nullptr};
    Verify(addresses, order);
  }
  {
    const char* const addresses[] = {"2002::1", "::ffff:1234:1", nullptr};
    Verify(addresses, order);
  }
}

// Rule 6: Prefer higher precedence.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule6) {
  AddMapping("::1", "::1");                       // loopback
  AddMapping("ff32::1", "fe81::10");              // multicast
  AddMapping("::ffff:1234:1", "::ffff:1234:10");  // IPv4-mapped
  AddMapping("2001::1", "2001::10");              // Teredo
  const char* const addresses[] = {"2001::1", "::ffff:1234:1", "ff32::1", "::1",
                                   nullptr};
  const int order[] = { 3, 2, 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 7: Prefer native transport.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule7) {
  AddMapping("3002::1", "4000::10");
  AddMapping("3002::2", "4000::20");
  GetSourceInfo("4000::20")->native = true;
  const char* const addresses[] = {"3002::1", "3002::2", nullptr};
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 8: Prefer smaller scope.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule8) {
  // Matching scope. Should precede the others by Rule 2.
  AddMapping("fe81::1", "fe81::10");  // link-local
  AddMapping("3000::1", "4000::10");  // global
  // Mismatched scope.
  AddMapping("ff32::1", "4000::10");  // link-local
  AddMapping("ff35::1", "4000::10");  // site-local
  AddMapping("ff38::1", "4000::10");  // org-local
  const char* const addresses[] = {"ff38::1", "3000::1", "ff35::1",
                                   "ff32::1", "fe81::1", nullptr};
  const int order[] = { 4, 1, 3, 2, 0, -1 };
  Verify(addresses, order);
}

// Rule 9: Use longest matching prefix.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule9) {
  AddMapping("3000::1", "3000:ffff::10");  // 16 bit match
  GetSourceInfo("3000:ffff::10")->prefix_length = 16;
  AddMapping("4000::1", "4000::10");       // 123 bit match, limited to 15
  GetSourceInfo("4000::10")->prefix_length = 15;
  AddMapping("4002::1", "4000::10");       // 14 bit match
  AddMapping("4080::1", "4000::10");       // 8 bit match
  const char* const addresses[] = {"4080::1", "4002::1", "4000::1", "3000::1",
                                   nullptr};
  const int order[] = { 3, 2, 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 10: Leave the order unchanged.
TEST_P(AddressSorterPosixSyncOrAsyncTest, Rule10) {
  AddMapping("4000::1", "4000::10");
  AddMapping("4000::2", "4000::10");
  AddMapping("4000::3", "4000::10");
  const char* const addresses[] = {"4000::1", "4000::2", "4000::3", nullptr};
  const int order[] = { 0, 1, 2, -1 };
  Verify(addresses, order);
}

TEST_P(AddressSorterPosixSyncOrAsyncTest, MultipleRules) {
  AddMapping("::1", "::1");           // loopback
  AddMapping("ff32::1", "fe81::10");  // link-local multicast
  AddMapping("ff3e::1", "4000::10");  // global multicast
  AddMapping("4000::1", "4000::10");  // global unicast
  AddMapping("ff32::2", "fe81::20");  // deprecated link-local multicast
  GetSourceInfo("fe81::20")->deprecated = true;
  const char* const addresses[] = {"ff3e::1", "ff32::2", "4000::1", "ff32::1",
                                   "::1",     "8.0.0.1", nullptr};
  const int order[] = { 4, 3, 0, 2, 1, -1 };
  Verify(addresses, order);
}

TEST_P(AddressSorterPosixSyncOrAsyncTest, InputPortsAreMaintained) {
  AddMapping("::1", "::1");
  AddMapping("::2", "::2");
  AddMapping("::3", "::3");

  IPEndPoint endpoint1(ParseIP("::1"), /*port=*/111);
  IPEndPoint endpoint2(ParseIP("::2"), /*port=*/222);
  IPEndPoint endpoint3(ParseIP("::3"), /*port=*/333);

  std::vector<IPEndPoint> input = {endpoint1, endpoint2, endpoint3};
  std::vector<IPEndPoint> sorted;
  TestCompletionCallback callback;
  sorter_->Sort(input, base::BindOnce(&OnSortComplete, std::ref(completed_),
                                      &sorted, callback.callback()));
  callback.WaitForResult();

  EXPECT_THAT(sorted, testing::ElementsAre(endpoint1, endpoint2, endpoint3));
}

TEST_P(AddressSorterPosixSyncOrAsyncTest, AddressSorterPosixDestroyed) {
  AddMapping("::1", "::1");
  AddMapping("::2", "::2");
  AddMapping("::3", "::3");

  IPEndPoint endpoint1(ParseIP("::1"), /*port=*/111);
  IPEndPoint endpoint2(ParseIP("::2"), /*port=*/222);
  IPEndPoint endpoint3(ParseIP("::3"), /*port=*/333);

  std::vector<IPEndPoint> input = {endpoint1, endpoint2, endpoint3};
  std::vector<IPEndPoint> sorted;
  TestCompletionCallback callback;
  sorter_->Sort(input, base::BindOnce(&OnSortComplete, std::ref(completed_),
                                      &sorted, callback.callback()));
  sorter_.reset();
  base::RunLoop().RunUntilIdle();

  TestUDPClientSocket::ConnectMode connect_mode = GetParam();
  if (connect_mode == TestUDPClientSocket::ConnectMode::kAsynchronous) {
    EXPECT_FALSE(completed_);
  } else {
    EXPECT_TRUE(completed_);
  }
}

TEST_F(AddressSorterPosixTest, RandomAsyncSocketOrder) {
  SetConnectMode(TestUDPClientSocket::ConnectMode::kAsynchronousManual);
  std::vector<TestUDPClientSocket*> created_sockets;
  SetSocketCreateCallback(base::BindRepeating(
      [](std::vector<TestUDPClientSocket*>& created_sockets,
         TestUDPClientSocket* socket) { created_sockets.push_back(socket); },
      std::ref(created_sockets)));

  AddMapping("::1", "::1");
  AddMapping("::2", "::2");
  AddMapping("::3", "::3");

  IPEndPoint endpoint1(ParseIP("::1"), /*port=*/111);
  IPEndPoint endpoint2(ParseIP("::2"), /*port=*/222);
  IPEndPoint endpoint3(ParseIP("::3"), /*port=*/333);

  std::vector<IPEndPoint> input = {endpoint1, endpoint2, endpoint3};
  std::vector<IPEndPoint> sorted;
  TestCompletionCallback callback;
  sorter_->Sort(input, base::BindOnce(&OnSortComplete, std::ref(completed_),
                                      &sorted, callback.callback()));

  ASSERT_EQ(created_sockets.size(), 3u);
  created_sockets[1]->FinishConnect();
  created_sockets[2]->FinishConnect();
  created_sockets[0]->FinishConnect();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(completed_);
}

// Regression test for https://crbug.com/1374387
TEST_F(AddressSorterPosixTest, IPAddressChangedSort) {
  SetConnectMode(TestUDPClientSocket::ConnectMode::kAsynchronousManual);
  std::vector<TestUDPClientSocket*> created_sockets;
  SetSocketCreateCallback(base::BindRepeating(
      [](std::vector<TestUDPClientSocket*>& created_sockets,
         TestUDPClientSocket* socket) { created_sockets.push_back(socket); },
      std::ref(created_sockets)));

  AddMapping("::1", "::1");
  AddMapping("::2", "::2");
  AddMapping("::3", "::3");

  IPEndPoint endpoint1(ParseIP("::1"), /*port=*/111);
  IPEndPoint endpoint2(ParseIP("::2"), /*port=*/222);
  IPEndPoint endpoint3(ParseIP("::3"), /*port=*/333);

  std::vector<IPEndPoint> input = {endpoint1, endpoint2, endpoint3};
  std::vector<IPEndPoint> sorted;
  TestCompletionCallback callback;
  sorter_->Sort(input, base::BindOnce(&OnSortComplete, std::ref(completed_),
                                      &sorted, callback.callback()));

  ASSERT_EQ(created_sockets.size(), 3u);
  created_sockets[0]->FinishConnect();
  // Trigger OnIPAddressChanged() to reset `source_map_`
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();
  created_sockets[1]->FinishConnect();
  created_sockets[2]->FinishConnect();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(completed_);
}

}  // namespace net
