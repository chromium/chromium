// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool_test_util.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

IPAddress ParseIP(const std::string& ip) {
  IPAddress address;
  CHECK(address.AssignFromIPLiteral(ip));
  return address;
}

// A StreamSocket which connects synchronously and successfully.
class MockConnectClientSocket : public TransportClientSocket {
 public:
  MockConnectClientSocket(const AddressList& addrlist, net::NetLog* net_log)
      : connected_(false),
        addrlist_(addrlist),
        net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)) {}

  // TransportClientSocket implementation.
  int Bind(const net::IPEndPoint& local_addr) override {
    NOTREACHED();
    return ERR_FAILED;
  }
  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override {
    connected_ = true;
    return OK;
  }
  void Disconnect() override { connected_ = false; }
  bool IsConnected() const override { return connected_; }
  bool IsConnectedAndIdle() const override { return connected_; }

  int GetPeerAddress(IPEndPoint* address) const override {
    *address = addrlist_.front();
    return OK;
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    if (!connected_)
      return ERR_SOCKET_NOT_CONNECTED;
    if (addrlist_.front().GetFamily() == ADDRESS_FAMILY_IPV4)
      SetIPv4Address(address);
    else
      SetIPv6Address(address);
    return OK;
  }
  const NetLogWithSource& NetLog() const override { return net_log_; }

  bool WasEverUsed() const override { return false; }
  bool WasAlpnNegotiated() const override { return false; }
  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }
  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }
  void GetConnectionAttempts(ConnectionAttempts* out) const override {
    out->clear();
  }
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {}
  int64_t GetTotalReceivedBytes() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  void ApplySocketTag(const SocketTag& tag) override {}

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return ERR_FAILED;
  }
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return ERR_FAILED;
  }
  int SetReceiveBufferSize(int32_t size) override { return OK; }
  int SetSendBufferSize(int32_t size) override { return OK; }

 private:
  bool connected_;
  const AddressList addrlist_;
  NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(MockConnectClientSocket);
};

class MockFailingClientSocket : public TransportClientSocket {
 public:
  MockFailingClientSocket(const AddressList& addrlist, net::NetLog* net_log)
      : addrlist_(addrlist),
        net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)) {}

  // TransportClientSocket implementation.
  int Bind(const net::IPEndPoint& local_addr) override {
    NOTREACHED();
    return ERR_FAILED;
  }

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override {
    return ERR_CONNECTION_FAILED;
  }

  void Disconnect() override {}

  bool IsConnected() const override { return false; }
  bool IsConnectedAndIdle() const override { return false; }
  int GetPeerAddress(IPEndPoint* address) const override {
    return ERR_UNEXPECTED;
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    return ERR_UNEXPECTED;
  }
  const NetLogWithSource& NetLog() const override { return net_log_; }

  bool WasEverUsed() const override { return false; }
  bool WasAlpnNegotiated() const override { return false; }
  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }
  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }
  void GetConnectionAttempts(ConnectionAttempts* out) const override {
    out->clear();
    for (const auto& addr : addrlist_)
      out->push_back(ConnectionAttempt(addr, ERR_CONNECTION_FAILED));
  }
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {}
  int64_t GetTotalReceivedBytes() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  void ApplySocketTag(const SocketTag& tag) override {}

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return ERR_FAILED;
  }

  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return ERR_FAILED;
  }
  int SetReceiveBufferSize(int32_t size) override { return OK; }
  int SetSendBufferSize(int32_t size) override { return OK; }

 private:
  const AddressList addrlist_;
  NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(MockFailingClientSocket);
};

class MockTriggerableClientSocket : public TransportClientSocket {
 public:
  // |should_connect| indicates whether the socket should successfully complete
  // or fail.
  MockTriggerableClientSocket(const AddressList& addrlist,
                              bool should_connect,
                              net::NetLog* net_log)
      : should_connect_(should_connect),
        is_connected_(false),
        addrlist_(addrlist),
        net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)) {}

  // Call this method to get a closure which will trigger the connect callback
  // when called. The closure can be called even after the socket is deleted; it
  // will safely do nothing.
  base::Closure GetConnectCallback() {
    return base::Bind(&MockTriggerableClientSocket::DoCallback,
                      weak_factory_.GetWeakPtr());
  }

  static std::unique_ptr<TransportClientSocket> MakeMockPendingClientSocket(
      const AddressList& addrlist,
      bool should_connect,
      net::NetLog* net_log) {
    std::unique_ptr<MockTriggerableClientSocket> socket(
        new MockTriggerableClientSocket(addrlist, should_connect, net_log));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  socket->GetConnectCallback());
    return std::move(socket);
  }

  static std::unique_ptr<TransportClientSocket> MakeMockDelayedClientSocket(
      const AddressList& addrlist,
      bool should_connect,
      const base::TimeDelta& delay,
      net::NetLog* net_log) {
    std::unique_ptr<MockTriggerableClientSocket> socket(
        new MockTriggerableClientSocket(addrlist, should_connect, net_log));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, socket->GetConnectCallback(), delay);
    return std::move(socket);
  }

  static std::unique_ptr<TransportClientSocket> MakeMockStalledClientSocket(
      const AddressList& addrlist,
      net::NetLog* net_log,
      bool failing) {
    std::unique_ptr<MockTriggerableClientSocket> socket(
        new MockTriggerableClientSocket(addrlist, true, net_log));
    if (failing) {
      DCHECK_LE(1u, addrlist.size());
      ConnectionAttempts attempts;
      attempts.push_back(ConnectionAttempt(addrlist[0], ERR_CONNECTION_FAILED));
      socket->AddConnectionAttempts(attempts);
    }
    return std::move(socket);
  }

  // TransportClientSocket implementation.
  int Bind(const net::IPEndPoint& local_addr) override {
    NOTREACHED();
    return ERR_FAILED;
  }

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override {
    DCHECK(callback_.is_null());
    callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  void Disconnect() override {}

  bool IsConnected() const override { return is_connected_; }
  bool IsConnectedAndIdle() const override { return is_connected_; }
  int GetPeerAddress(IPEndPoint* address) const override {
    *address = addrlist_.front();
    return OK;
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    if (!is_connected_)
      return ERR_SOCKET_NOT_CONNECTED;
    if (addrlist_.front().GetFamily() == ADDRESS_FAMILY_IPV4)
      SetIPv4Address(address);
    else
      SetIPv6Address(address);
    return OK;
  }
  const NetLogWithSource& NetLog() const override { return net_log_; }

  bool WasEverUsed() const override { return false; }
  bool WasAlpnNegotiated() const override { return false; }
  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }
  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }
  void GetConnectionAttempts(ConnectionAttempts* out) const override {
    *out = connection_attempts_;
  }
  void ClearConnectionAttempts() override { connection_attempts_.clear(); }
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {
    connection_attempts_.insert(connection_attempts_.begin(), attempts.begin(),
                                attempts.end());
  }
  int64_t GetTotalReceivedBytes() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  void ApplySocketTag(const SocketTag& tag) override {}

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return ERR_FAILED;
  }

  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return ERR_FAILED;
  }
  int SetReceiveBufferSize(int32_t size) override { return OK; }
  int SetSendBufferSize(int32_t size) override { return OK; }

 private:
  void DoCallback() {
    is_connected_ = should_connect_;
    std::move(callback_).Run(is_connected_ ? OK : ERR_CONNECTION_FAILED);
  }

  bool should_connect_;
  bool is_connected_;
  const AddressList addrlist_;
  NetLogWithSource net_log_;
  CompletionOnceCallback callback_;
  ConnectionAttempts connection_attempts_;

  base::WeakPtrFactory<MockTriggerableClientSocket> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockTriggerableClientSocket);
};

}  // namespace

void TestLoadTimingInfoConnectedReused(const ClientSocketHandle& handle) {
  LoadTimingInfo load_timing_info;
  // Only pass true in as |is_reused|, as in general, HttpStream types should
  // have stricter concepts of reuse than socket pools.
  EXPECT_TRUE(handle.GetLoadTimingInfo(true, &load_timing_info));

  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

void TestLoadTimingInfoConnectedNotReused(const ClientSocketHandle& handle) {
  EXPECT_FALSE(handle.is_reused());

  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(handle.GetLoadTimingInfo(false, &load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLogSource::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);

  TestLoadTimingInfoConnectedReused(handle);
}

void SetIPv4Address(IPEndPoint* address) {
  *address = IPEndPoint(ParseIP("1.1.1.1"), 80);
}

void SetIPv6Address(IPEndPoint* address) {
  *address = IPEndPoint(ParseIP("1:abcd::3:4:ff"), 80);
}

MockTransportClientSocketFactory::MockTransportClientSocketFactory(
    NetLog* net_log)
    : net_log_(net_log),
      allocation_count_(0),
      client_socket_type_(MOCK_CLIENT_SOCKET),
      client_socket_types_(nullptr),
      client_socket_index_(0),
      client_socket_index_max_(0),
      delay_(base::TimeDelta::FromMilliseconds(
          ClientSocketPool::kMaxConnectRetryIntervalMs)) {}

MockTransportClientSocketFactory::~MockTransportClientSocketFactory() = default;

std::unique_ptr<DatagramClientSocket>
MockTransportClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    NetLog* net_log,
    const NetLogSource& source) {
  NOTREACHED();
  return std::unique_ptr<DatagramClientSocket>();
}

std::unique_ptr<TransportClientSocket>
MockTransportClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    std::unique_ptr<SocketPerformanceWatcher> /* socket_performance_watcher */,
    NetLog* /* net_log */,
    const NetLogSource& /* source */) {
  allocation_count_++;

  ClientSocketType type = client_socket_type_;
  if (client_socket_types_ && client_socket_index_ < client_socket_index_max_) {
    type = client_socket_types_[client_socket_index_++];
  }

  switch (type) {
    case MOCK_CLIENT_SOCKET:
      return std::make_unique<MockConnectClientSocket>(addresses, net_log_);
    case MOCK_FAILING_CLIENT_SOCKET:
      return std::make_unique<MockFailingClientSocket>(addresses, net_log_);
    case MOCK_PENDING_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockPendingClientSocket(
          addresses, true, net_log_);
    case MOCK_PENDING_FAILING_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockPendingClientSocket(
          addresses, false, net_log_);
    case MOCK_DELAYED_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockDelayedClientSocket(
          addresses, true, delay_, net_log_);
    case MOCK_DELAYED_FAILING_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockDelayedClientSocket(
          addresses, false, delay_, net_log_);
    case MOCK_STALLED_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockStalledClientSocket(
          addresses, net_log_, false);
    case MOCK_STALLED_FAILING_CLIENT_SOCKET:
      return MockTriggerableClientSocket::MakeMockStalledClientSocket(
          addresses, net_log_, true);
    case MOCK_TRIGGERABLE_CLIENT_SOCKET: {
      std::unique_ptr<MockTriggerableClientSocket> rv(
          new MockTriggerableClientSocket(addresses, true, net_log_));
      triggerable_sockets_.push(rv->GetConnectCallback());
      // run_loop_quit_closure_ behaves like a condition variable. It will
      // wake up WaitForTriggerableSocketCreation() if it is sleeping. We
      // don't need to worry about atomicity because this code is
      // single-threaded.
      if (!run_loop_quit_closure_.is_null())
        run_loop_quit_closure_.Run();
      return std::move(rv);
    }
    default:
      NOTREACHED();
      return std::make_unique<MockConnectClientSocket>(addresses, net_log_);
  }
}

std::unique_ptr<SSLClientSocket>
MockTransportClientSocketFactory::CreateSSLClientSocket(
    SSLClientContext* context,
    std::unique_ptr<StreamSocket> stream_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<ProxyClientSocket>
MockTransportClientSocketFactory::CreateProxyClientSocket(
    std::unique_ptr<StreamSocket> stream_socket,
    const std::string& user_agent,
    const HostPortPair& endpoint,
    const ProxyServer& proxy_server,
    HttpAuthController* http_auth_controller,
    bool tunnel,
    bool using_spdy,
    NextProto negotiated_protocol,
    ProxyDelegate* proxy_delegate,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  NOTIMPLEMENTED();
  return nullptr;
}

void MockTransportClientSocketFactory::set_client_socket_types(
    ClientSocketType* type_list,
    int num_types) {
  DCHECK_GT(num_types, 0);
  client_socket_types_ = type_list;
  client_socket_index_ = 0;
  client_socket_index_max_ = num_types;
}

base::Closure
MockTransportClientSocketFactory::WaitForTriggerableSocketCreation() {
  while (triggerable_sockets_.empty()) {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_closure_.Reset();
  }
  base::Closure trigger = triggerable_sockets_.front();
  triggerable_sockets_.pop();
  return trigger;
}

}  // namespace net
