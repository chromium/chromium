// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool_test_util.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
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
      : addrlist_(addrlist),
        net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)) {}

  MockConnectClientSocket(const MockConnectClientSocket&) = delete;
  MockConnectClientSocket& operator=(const MockConnectClientSocket&) = delete;

  // TransportClientSocket implementation.
  int Bind(const net::IPEndPoint& local_addr) override {
    NOTREACHED_IN_MIGRATION();
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
  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }
  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }
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
  bool connected_ = false;
  const AddressList addrlist_;
  NetLogWithSource net_log_;
};

class MockFailingClientSocket : public TransportClientSocket {
 public:
  MockFailingClientSocket(const AddressList& addrlist,
                          Error connect_error,
                          net::NetLog* net_log)
      : addrlist_(addrlist),
        connect_error_(connect_error),
        net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)) {}

  MockFailingClientSocket(const MockFailingClientSocket&) = delete;
  MockFailingClientSocket& operator=(const MockFailingClientSocket&) = delete;

  // TransportClientSocket implementation.
  int Bind(const net::IPEndPoint& local_addr) override {
    NOTREACHED_IN_MIGRATION();
    return ERR_FAILED;
  }

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override {
    return connect_error_;
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
  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }
  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }
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
  const Error connect_error_;
  NetLogWithSource net_log_;
};

class MockTriggerableClientSocket : public TransportClientSocket {
 public:
  // |connect_error| indicates whether the socket should successfully complete
  // or fail.
  MockTriggerableClientSocket(const AddressList& addrlist,
                              Error connect_error,
                              net::NetLog* net_log)
      : connect_error_(connect_error),
        addrlist_(addrlist),
        net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::SOCKET)) {}

  MockTriggerableClientSocket(const MockTriggerableClientSocket&) = delete;
  MockTriggerableClientSocket& operator=(const MockTriggerableClientSocket&) =
      delete;

  // Call this method to get a closure which will trigger the connect callback
  // when called. The closure can be called even after the socket is deleted; it
  // will safely do nothing.
  base::OnceClosure GetConnectCallback() {
    return base::BindOnce(&MockTriggerableClientSocket::DoCallback,
                          weak_factory_.GetWeakPtr());
  }

  static std::unique_ptr<TransportClientSocket> MakeMockPendingClientSocket(
      const AddressList& addrlist,
      Error connect_error,
      net::NetLog* net_log) {
    auto socket = std::make_unique<MockTriggerableClientSocket>(
        addrlist, connect_error, net_log);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, socket->GetConnectCallback());
    return std::move(socket);
  }

  static std::unique_ptr<TransportClientSocket> MakeMockDelayedClientSocket(
      const AddressList& addrlist,
      Error connect_error,
      const base::TimeDelta& delay,
      net::NetLog* net_log) {
    auto socket = std::make_unique<MockTriggerableClientSocket>(
        addrlist, connect_error, net_log);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, socket->GetConnectCallback(), delay);
    return std::move(socket);
  }

  static std::unique_ptr<TransportClientSocket> MakeMockStalledClientSocket(
      const AddressList& addrlist,
      net::NetLog* net_log) {
    // We never post `GetConnectCallback()`, so the value of `connect_error`
    // does not matter.
    return std::make_unique<MockTriggerableClientSocket>(
        addrlist, /*connect_error=*/OK, net_log);
  }

  // TransportClientSocket implementation.
  int Bind(const net::IPEndPoint& local_addr) override {
    NOTREACHED_IN_MIGRATION();
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
  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }
  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }
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
    is_connected_ = connect_error_ == OK;
    std::move(callback_).Run(connect_error_);
  }

  Error connect_error_;
  bool is_connected_ = false;
  const AddressList addrlist_;
  NetLogWithSource net_log_;
  CompletionOnceCallback callback_;

  base::WeakPtrFactory<MockTriggerableClientSocket> weak_factory_{this};
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

MockTransportClientSocketFactory::Rule::Rule(
    Type type,
    std::optional<std::vector<IPEndPoint>> expected_addresses,
    Error connect_error)
    : type(type),
      expected_addresses(std::move(expected_addresses)),
      connect_error(connect_error) {}

MockTransportClientSocketFactory::Rule::~Rule() = default;

MockTransportClientSocketFactory::Rule::Rule(const Rule&) = default;

MockTransportClientSocketFactory::Rule&
MockTransportClientSocketFactory::Rule::operator=(const Rule&) = default;

MockTransportClientSocketFactory::MockTransportClientSocketFactory(
    NetLog* net_log)
    : net_log_(net_log),
      delay_(base::Milliseconds(ClientSocketPool::kMaxConnectRetryIntervalMs)) {
}

MockTransportClientSocketFactory::~MockTransportClientSocketFactory() = default;

std::unique_ptr<DatagramClientSocket>
MockTransportClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    NetLog* net_log,
    const NetLogSource& source) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::unique_ptr<TransportClientSocket>
MockTransportClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    std::unique_ptr<SocketPerformanceWatcher> /* socket_performance_watcher */,
    NetworkQualityEstimator* /* network_quality_estimator */,
    NetLog* /* net_log */,
    const NetLogSource& /* source */) {
  allocation_count_++;

  Rule rule(client_socket_type_);
  if (!rules_.empty()) {
    rule = rules_.front();
    rules_ = rules_.subspan(1);
  }

  if (rule.expected_addresses) {
    EXPECT_EQ(addresses.endpoints(), *rule.expected_addresses);
  }

  switch (rule.type) {
    case Type::kUnexpected:
      ADD_FAILURE() << "Unexpectedly created socket to "
                    << addresses.endpoints().front();
      return std::make_unique<MockConnectClientSocket>(addresses, net_log_);
    case Type::kSynchronous:
      return std::make_unique<MockConnectClientSocket>(addresses, net_log_);
    case Type::kFailing:
      return std::make_unique<MockFailingClientSocket>(
          addresses, rule.connect_error, net_log_);
    case Type::kPending:
      return MockTriggerableClientSocket::MakeMockPendingClientSocket(
          addresses, OK, net_log_);
    case Type::kPendingFailing:
      return MockTriggerableClientSocket::MakeMockPendingClientSocket(
          addresses, rule.connect_error, net_log_);
    case Type::kDelayed:
      return MockTriggerableClientSocket::MakeMockDelayedClientSocket(
          addresses, OK, delay_, net_log_);
    case Type::kDelayedFailing:
      return MockTriggerableClientSocket::MakeMockDelayedClientSocket(
          addresses, rule.connect_error, delay_, net_log_);
    case Type::kStalled:
      return MockTriggerableClientSocket::MakeMockStalledClientSocket(addresses,
                                                                      net_log_);
    case Type::kTriggerable: {
      auto rv = std::make_unique<MockTriggerableClientSocket>(addresses, OK,
                                                              net_log_);
      triggerable_sockets_.push(rv->GetConnectCallback());
      // run_loop_quit_closure_ behaves like a condition variable. It will
      // wake up WaitForTriggerableSocketCreation() if it is sleeping. We
      // don't need to worry about atomicity because this code is
      // single-threaded.
      if (!run_loop_quit_closure_.is_null())
        std::move(run_loop_quit_closure_).Run();
      return std::move(rv);
    }
    default:
      NOTREACHED_IN_MIGRATION();
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

void MockTransportClientSocketFactory::SetRules(base::span<const Rule> rules) {
  DCHECK(rules_.empty());
  client_socket_type_ = Type::kUnexpected;
  rules_ = rules;
}

base::OnceClosure
MockTransportClientSocketFactory::WaitForTriggerableSocketCreation() {
  while (triggerable_sockets_.empty()) {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_closure_.Reset();
  }
  base::OnceClosure trigger = std::move(triggerable_sockets_.front());
  triggerable_sockets_.pop();
  return trigger;
}

}  // namespace net
