// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter_posix.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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
  explicit TestUDPClientSocket(const AddressMapping* mapping)
      : mapping_(mapping), connected_(false)  {}

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
  int WriteAsync(
      const char* buffer,
      size_t buf_len,
      CompletionOnceCallback callback,
      const NetworkTrafficAnnotationTag& traffic_annotation) override {
    NOTIMPLEMENTED();
    return OK;
  }
  int WriteAsync(
      DatagramBuffers buffers,
      CompletionOnceCallback callback,
      const NetworkTrafficAnnotationTag& traffic_annotation) override {
    NOTIMPLEMENTED();
    return OK;
  }
  DatagramBuffers GetUnwrittenBuffers() override {
    DatagramBuffers result;
    NOTIMPLEMENTED();
    return result;
  }
  void SetWriteAsyncEnabled(bool enabled) override {}
  void SetMaxPacketSize(size_t max_packet_size) override {}
  bool WriteAsyncEnabled() override { return false; }
  void SetWriteMultiCoreEnabled(bool enabled) override {}
  void SetSendmmsgEnabled(bool enabled) override {}
  void SetWriteBatchingActive(bool active) override {}
  int SetMulticastInterface(uint32_t interface_index) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }

  int ConnectUsingNetwork(NetworkChangeNotifier::NetworkHandle network,
                          const IPEndPoint& address) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override {
    NOTIMPLEMENTED();
    return ERR_NOT_IMPLEMENTED;
  }
  NetworkChangeNotifier::NetworkHandle GetBoundNetwork() const override {
    return NetworkChangeNotifier::kInvalidNetworkHandle;
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

 private:
  NetLogWithSource net_log_;
  const AddressMapping* mapping_;
  bool connected_;
  IPEndPoint local_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(TestUDPClientSocket);
};

// Creates TestUDPClientSockets and maintains an AddressMapping.
class TestSocketFactory : public ClientSocketFactory {
 public:
  TestSocketFactory() = default;
  ~TestSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType,
      NetLog*,
      const NetLogSource&) override {
    return std::unique_ptr<DatagramClientSocket>(
        new TestUDPClientSocket(&mapping_));
  }
  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList&,
      std::unique_ptr<SocketPerformanceWatcher>,
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
  std::unique_ptr<ProxyClientSocket> CreateProxyClientSocket(
      std::unique_ptr<StreamSocket> stream_socket,
      const std::string& user_agent,
      const HostPortPair& endpoint,
      const ProxyServer& proxy_server,
      HttpAuthController* http_auth_controller,
      bool tunnel,
      bool using_spdy,
      NextProto negotiated_protocol,
      ProxyDelegate* proxy_delegate,
      const NetworkTrafficAnnotationTag& traffic_annotation) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  void AddMapping(const IPAddress& dst, const IPAddress& src) {
    mapping_[dst] = src;
  }

 private:
  AddressMapping mapping_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketFactory);
};

void OnSortComplete(AddressList* result_buf,
                    CompletionOnceCallback callback,
                    bool success,
                    const AddressList& result) {
  EXPECT_TRUE(success);
  if (success)
    *result_buf = result;
  std::move(callback).Run(OK);
}

}  // namespace

class AddressSorterPosixTest : public testing::Test {
 protected:
  AddressSorterPosixTest() : sorter_(&socket_factory_) {}

  void AddMapping(const std::string& dst, const std::string& src) {
    socket_factory_.AddMapping(ParseIP(dst), ParseIP(src));
  }

  AddressSorterPosix::SourceAddressInfo* GetSourceInfo(
      const std::string& addr) {
    IPAddress address = ParseIP(addr);
    AddressSorterPosix::SourceAddressInfo* info = &sorter_.source_map_[address];
    if (info->scope == AddressSorterPosix::SCOPE_UNDEFINED)
      sorter_.FillPolicy(address, info);
    return info;
  }

  // Verify that NULL-terminated |addresses| matches (-1)-terminated |order|
  // after sorting.
  void Verify(const char* const addresses[], const int order[]) {
    AddressList list;
    for (const char* const* addr = addresses; *addr != NULL; ++addr)
      list.push_back(IPEndPoint(ParseIP(*addr), 80));
    for (size_t i = 0; order[i] >= 0; ++i)
      CHECK_LT(order[i], static_cast<int>(list.size()));

    AddressList result;
    TestCompletionCallback callback;
    sorter_.Sort(list,
                 base::BindOnce(&OnSortComplete, &result, callback.callback()));
    callback.WaitForResult();

    for (size_t i = 0; (i < result.size()) || (order[i] >= 0); ++i) {
      IPEndPoint expected = order[i] >= 0 ? list[order[i]] : IPEndPoint();
      IPEndPoint actual = i < result.size() ? result[i] : IPEndPoint();
      EXPECT_TRUE(expected.address() == actual.address()) <<
          "Address out of order at position " << i << "\n" <<
          "  Actual: " << actual.ToStringWithoutPort() << "\n" <<
          "Expected: " << expected.ToStringWithoutPort();
    }
  }

  TestSocketFactory socket_factory_;
  AddressSorterPosix sorter_;
};

// Rule 1: Avoid unusable destinations.
TEST_F(AddressSorterPosixTest, Rule1) {
  AddMapping("10.0.0.231", "10.0.0.1");
  const char* const addresses[] = { "::1", "10.0.0.231", "127.0.0.1", NULL };
  const int order[] = { 1, -1 };
  Verify(addresses, order);
}

// Rule 2: Prefer matching scope.
TEST_F(AddressSorterPosixTest, Rule2) {
  AddMapping("3002::1", "4000::10");      // matching global
  AddMapping("ff32::1", "fe81::10");      // matching link-local
  AddMapping("fec1::1", "fec1::10");      // matching node-local
  AddMapping("3002::2", "::1");           // global vs. link-local
  AddMapping("fec1::2", "fe81::10");      // site-local vs. link-local
  AddMapping("8.0.0.1", "169.254.0.10");  // global vs. link-local
  // In all three cases, matching scope is preferred.
  const int order[] = { 1, 0, -1 };
  const char* const addresses1[] = { "3002::2", "3002::1", NULL };
  Verify(addresses1, order);
  const char* const addresses2[] = { "fec1::2", "ff32::1", NULL };
  Verify(addresses2, order);
  const char* const addresses3[] = { "8.0.0.1", "fec1::1", NULL };
  Verify(addresses3, order);
}

// Rule 3: Avoid deprecated addresses.
TEST_F(AddressSorterPosixTest, Rule3) {
  // Matching scope.
  AddMapping("3002::1", "4000::10");
  GetSourceInfo("4000::10")->deprecated = true;
  AddMapping("3002::2", "4000::20");
  const char* const addresses[] = { "3002::1", "3002::2", NULL };
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 4: Prefer home addresses.
TEST_F(AddressSorterPosixTest, Rule4) {
  AddMapping("3002::1", "4000::10");
  AddMapping("3002::2", "4000::20");
  GetSourceInfo("4000::20")->home = true;
  const char* const addresses[] = { "3002::1", "3002::2", NULL };
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 5: Prefer matching label.
TEST_F(AddressSorterPosixTest, Rule5) {
  AddMapping("::1", "::1");                       // matching loopback
  AddMapping("::ffff:1234:1", "::ffff:1234:10");  // matching IPv4-mapped
  AddMapping("2001::1", "::ffff:1234:10");        // Teredo vs. IPv4-mapped
  AddMapping("2002::1", "2001::10");              // 6to4 vs. Teredo
  const int order[] = { 1, 0, -1 };
  {
    const char* const addresses[] = { "2001::1", "::1", NULL };
    Verify(addresses, order);
  }
  {
    const char* const addresses[] = { "2002::1", "::ffff:1234:1", NULL };
    Verify(addresses, order);
  }
}

// Rule 6: Prefer higher precedence.
TEST_F(AddressSorterPosixTest, Rule6) {
  AddMapping("::1", "::1");                       // loopback
  AddMapping("ff32::1", "fe81::10");              // multicast
  AddMapping("::ffff:1234:1", "::ffff:1234:10");  // IPv4-mapped
  AddMapping("2001::1", "2001::10");              // Teredo
  const char* const addresses[] = { "2001::1", "::ffff:1234:1", "ff32::1",
    "::1", NULL };
  const int order[] = { 3, 2, 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 7: Prefer native transport.
TEST_F(AddressSorterPosixTest, Rule7) {
  AddMapping("3002::1", "4000::10");
  AddMapping("3002::2", "4000::20");
  GetSourceInfo("4000::20")->native = true;
  const char* const addresses[] = { "3002::1", "3002::2", NULL };
  const int order[] = { 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 8: Prefer smaller scope.
TEST_F(AddressSorterPosixTest, Rule8) {
  // Matching scope. Should precede the others by Rule 2.
  AddMapping("fe81::1", "fe81::10");  // link-local
  AddMapping("3000::1", "4000::10");  // global
  // Mismatched scope.
  AddMapping("ff32::1", "4000::10");  // link-local
  AddMapping("ff35::1", "4000::10");  // site-local
  AddMapping("ff38::1", "4000::10");  // org-local
  const char* const addresses[] = { "ff38::1", "3000::1", "ff35::1", "ff32::1",
                                    "fe81::1", NULL };
  const int order[] = { 4, 1, 3, 2, 0, -1 };
  Verify(addresses, order);
}

// Rule 9: Use longest matching prefix.
TEST_F(AddressSorterPosixTest, Rule9) {
  AddMapping("3000::1", "3000:ffff::10");  // 16 bit match
  GetSourceInfo("3000:ffff::10")->prefix_length = 16;
  AddMapping("4000::1", "4000::10");       // 123 bit match, limited to 15
  GetSourceInfo("4000::10")->prefix_length = 15;
  AddMapping("4002::1", "4000::10");       // 14 bit match
  AddMapping("4080::1", "4000::10");       // 8 bit match
  const char* const addresses[] = { "4080::1", "4002::1", "4000::1", "3000::1",
                                    NULL };
  const int order[] = { 3, 2, 1, 0, -1 };
  Verify(addresses, order);
}

// Rule 10: Leave the order unchanged.
TEST_F(AddressSorterPosixTest, Rule10) {
  AddMapping("4000::1", "4000::10");
  AddMapping("4000::2", "4000::10");
  AddMapping("4000::3", "4000::10");
  const char* const addresses[] = { "4000::1", "4000::2", "4000::3", NULL };
  const int order[] = { 0, 1, 2, -1 };
  Verify(addresses, order);
}

TEST_F(AddressSorterPosixTest, MultipleRules) {
  AddMapping("::1", "::1");           // loopback
  AddMapping("ff32::1", "fe81::10");  // link-local multicast
  AddMapping("ff3e::1", "4000::10");  // global multicast
  AddMapping("4000::1", "4000::10");  // global unicast
  AddMapping("ff32::2", "fe81::20");  // deprecated link-local multicast
  GetSourceInfo("fe81::20")->deprecated = true;
  const char* const addresses[] = { "ff3e::1", "ff32::2", "4000::1", "ff32::1",
                                    "::1", "8.0.0.1", NULL };
  const int order[] = { 4, 3, 0, 2, 1, -1 };
  Verify(addresses, order);
}

}  // namespace net
