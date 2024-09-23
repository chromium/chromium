// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks_client_socket.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsError;
using net::test::IsOk;

//-----------------------------------------------------------------------------

namespace net {

class NetLog;

class SOCKSClientSocketTest : public PlatformTest, public WithTaskEnvironment {
 public:
  SOCKSClientSocketTest();
  // Create a SOCKSClientSocket on top of a MockSocket.
  std::unique_ptr<SOCKSClientSocket> BuildMockSocket(
      base::span<const MockRead> reads,
      base::span<const MockWrite> writes,
      HostResolver* host_resolver,
      const std::string& hostname,
      int port,
      NetLog* net_log);
  void SetUp() override;

 protected:
  std::unique_ptr<MockHostResolver> host_resolver_;
  std::unique_ptr<SocketDataProvider> data_;
  std::unique_ptr<SOCKSClientSocket> user_sock_;
  AddressList address_list_;
  // Filled in by BuildMockSocket() and owned by its return value
  // (which |user_sock| is set to).
  raw_ptr<StreamSocket> tcp_sock_;
  TestCompletionCallback callback_;
};

SOCKSClientSocketTest::SOCKSClientSocketTest()
    : host_resolver_(std::make_unique<MockHostResolver>()) {}

// Set up platform before every test case
void SOCKSClientSocketTest::SetUp() {
  PlatformTest::SetUp();
}

std::unique_ptr<SOCKSClientSocket> SOCKSClientSocketTest::BuildMockSocket(
    base::span<const MockRead> reads,
    base::span<const MockWrite> writes,
    HostResolver* host_resolver,
    const std::string& hostname,
    int port,
    NetLog* net_log) {
  TestCompletionCallback callback;
  data_ = std::make_unique<StaticSocketDataProvider>(reads, writes);
  auto socket = std::make_unique<MockTCPClientSocket>(address_list_, net_log,
                                                      data_.get());
  socket->set_enable_read_if_ready(true);

  int rv = socket->Connect(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(socket->IsConnected());

  // The SOCKSClientSocket takes ownership of |socket|, but |tcp_sock_| keeps a
  // non-owning pointer to it.
  tcp_sock_ = socket.get();
  return std::make_unique<SOCKSClientSocket>(
      std::move(socket), HostPortPair(hostname, port),
      NetworkAnonymizationKey(), DEFAULT_PRIORITY, host_resolver,
      SecureDnsPolicy::kAllow, TRAFFIC_ANNOTATION_FOR_TESTS);
}

// Tests a complete handshake and the disconnection.
TEST_F(SOCKSClientSocketTest, CompleteHandshake) {
  // Run the test twice. Once with ReadIfReady() and once with Read().
  for (bool use_read_if_ready : {true, false}) {
    const std::string payload_write = "random data";
    const std::string payload_read = "moar random data";

    MockWrite data_writes[] = {
        MockWrite(ASYNC, kSOCKS4OkRequestLocalHostPort80,
                  kSOCKS4OkRequestLocalHostPort80Length),
        MockWrite(ASYNC, payload_write.data(), payload_write.size())};
    MockRead data_reads[] = {
        MockRead(ASYNC, kSOCKS4OkReply, kSOCKS4OkReplyLength),
        MockRead(ASYNC, payload_read.data(), payload_read.size())};
    RecordingNetLogObserver log_observer;

    user_sock_ = BuildMockSocket(data_reads, data_writes, host_resolver_.get(),
                                 "localhost", 80, NetLog::Get());

    // At this state the TCP connection is completed but not the SOCKS
    // handshake.
    EXPECT_TRUE(tcp_sock_->IsConnected());
    EXPECT_FALSE(user_sock_->IsConnected());

    int rv = user_sock_->Connect(callback_.callback());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

    auto entries = log_observer.GetEntries();
    EXPECT_TRUE(
        LogContainsBeginEvent(entries, 0, NetLogEventType::SOCKS_CONNECT));
    EXPECT_FALSE(user_sock_->IsConnected());

    rv = callback_.WaitForResult();
    EXPECT_THAT(rv, IsOk());
    EXPECT_TRUE(user_sock_->IsConnected());
    entries = log_observer.GetEntries();
    EXPECT_TRUE(
        LogContainsEndEvent(entries, -1, NetLogEventType::SOCKS_CONNECT));

    auto buffer = base::MakeRefCounted<IOBufferWithSize>(payload_write.size());
    memcpy(buffer->data(), payload_write.data(), payload_write.size());
    rv = user_sock_->Write(buffer.get(), payload_write.size(),
                           callback_.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    rv = callback_.WaitForResult();
    EXPECT_EQ(static_cast<int>(payload_write.size()), rv);

    buffer = base::MakeRefCounted<IOBufferWithSize>(payload_read.size());
    if (use_read_if_ready) {
      rv = user_sock_->ReadIfReady(buffer.get(), payload_read.size(),
                                   callback_.callback());
      EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
      rv = callback_.WaitForResult();
      EXPECT_EQ(net::OK, rv);
      rv = user_sock_->ReadIfReady(buffer.get(), payload_read.size(),
                                   callback_.callback());
    } else {
      rv = user_sock_->Read(buffer.get(), payload_read.size(),
                            callback_.callback());
      EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
      rv = callback_.WaitForResult();
    }
    EXPECT_EQ(static_cast<int>(payload_read.size()), rv);
    EXPECT_EQ(payload_read, std::string(buffer->data(), payload_read.size()));

    user_sock_->Disconnect();
    EXPECT_FALSE(tcp_sock_->IsConnected());
    EXPECT_FALSE(user_sock_->IsConnected());
  }
}

TEST_F(SOCKSClientSocketTest, CancelPendingReadIfReady) {
  const std::string payload_read = "random data";

  MockWrite data_writes[] = {MockWrite(ASYNC, kSOCKS4OkRequestLocalHostPort80,
                                       kSOCKS4OkRequestLocalHostPort80Length)};
  MockRead data_reads[] = {
      MockRead(ASYNC, kSOCKS4OkReply, kSOCKS4OkReplyLength),
      MockRead(ASYNC, payload_read.data(), payload_read.size())};
  user_sock_ = BuildMockSocket(data_reads, data_writes, host_resolver_.get(),
                               "localhost", 80, nullptr);

  // At this state the TCP connection is completed but not the SOCKS
  // handshake.
  EXPECT_TRUE(tcp_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnected());

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = callback_.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(user_sock_->IsConnected());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(payload_read.size());
  rv = user_sock_->ReadIfReady(buffer.get(), payload_read.size(),
                               callback_.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  rv = user_sock_->CancelReadIfReady();
  EXPECT_EQ(net::OK, rv);

  user_sock_->Disconnect();
  EXPECT_FALSE(tcp_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnected());
}

// List of responses from the socks server and the errors they should
// throw up are tested here.
TEST_F(SOCKSClientSocketTest, HandshakeFailures) {
  const struct {
    const char fail_reply[8];
    Error fail_code;
  } tests[] = {
    // Failure of the server response code
    {
      { 0x01, 0x5A, 0x00, 0x00, 0, 0, 0, 0 },
      ERR_SOCKS_CONNECTION_FAILED,
    },
    // Failure of the null byte
    {
      { 0x00, 0x5B, 0x00, 0x00, 0, 0, 0, 0 },
      ERR_SOCKS_CONNECTION_FAILED,
    },
  };

  //---------------------------------------
  host_resolver_->rules()->AddRule("socks.test", "127.0.0.1");
  for (const auto& test : tests) {
    MockWrite data_writes[] = {
        MockWrite(SYNCHRONOUS, kSOCKS4OkRequestLocalHostPort80,
                  kSOCKS4OkRequestLocalHostPort80Length)};
    MockRead data_reads[] = {
        MockRead(SYNCHRONOUS, test.fail_reply, std::size(test.fail_reply))};
    RecordingNetLogObserver log_observer;

    user_sock_ = BuildMockSocket(data_reads, data_writes, host_resolver_.get(),
                                 "socks.test", 80, NetLog::Get());

    int rv = user_sock_->Connect(callback_.callback());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

    auto entries = log_observer.GetEntries();
    EXPECT_TRUE(
        LogContainsBeginEvent(entries, 0, NetLogEventType::SOCKS_CONNECT));

    rv = callback_.WaitForResult();
    EXPECT_EQ(test.fail_code, rv);
    EXPECT_FALSE(user_sock_->IsConnected());
    EXPECT_TRUE(tcp_sock_->IsConnected());
    entries = log_observer.GetEntries();
    EXPECT_TRUE(
        LogContainsEndEvent(entries, -1, NetLogEventType::SOCKS_CONNECT));
  }
}

// Tests scenario when the server sends the handshake response in
// more than one packet.
TEST_F(SOCKSClientSocketTest, PartialServerReads) {
  const char kSOCKSPartialReply1[] = { 0x00 };
  const char kSOCKSPartialReply2[] = { 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

  MockWrite data_writes[] = {MockWrite(ASYNC, kSOCKS4OkRequestLocalHostPort80,
                                       kSOCKS4OkRequestLocalHostPort80Length)};
  MockRead data_reads[] = {
      MockRead(ASYNC, kSOCKSPartialReply1, std::size(kSOCKSPartialReply1)),
      MockRead(ASYNC, kSOCKSPartialReply2, std::size(kSOCKSPartialReply2))};
  RecordingNetLogObserver log_observer;

  user_sock_ = BuildMockSocket(data_reads, data_writes, host_resolver_.get(),
                               "localhost", 80, NetLog::Get());

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  auto entries = log_observer.GetEntries();
  EXPECT_TRUE(
      LogContainsBeginEvent(entries, 0, NetLogEventType::SOCKS_CONNECT));

  rv = callback_.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(user_sock_->IsConnected());
  entries = log_observer.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SOCKS_CONNECT));
}

// Tests scenario when the client sends the handshake request in
// more than one packet.
TEST_F(SOCKSClientSocketTest, PartialClientWrites) {
  const char kSOCKSPartialRequest1[] = { 0x04, 0x01 };
  const char kSOCKSPartialRequest2[] = { 0x00, 0x50, 127, 0, 0, 1, 0 };

  MockWrite data_writes[] = {
      MockWrite(ASYNC, kSOCKSPartialRequest1, std::size(kSOCKSPartialRequest1)),
      // simulate some empty writes
      MockWrite(ASYNC, 0),
      MockWrite(ASYNC, 0),
      MockWrite(ASYNC, kSOCKSPartialRequest2, std::size(kSOCKSPartialRequest2)),
  };
  MockRead data_reads[] = {
      MockRead(ASYNC, kSOCKS4OkReply, kSOCKS4OkReplyLength)};
  RecordingNetLogObserver log_observer;

  user_sock_ = BuildMockSocket(data_reads, data_writes, host_resolver_.get(),
                               "localhost", 80, NetLog::Get());

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  auto entries = log_observer.GetEntries();
  EXPECT_TRUE(
      LogContainsBeginEvent(entries, 0, NetLogEventType::SOCKS_CONNECT));

  rv = callback_.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(user_sock_->IsConnected());
  entries = log_observer.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SOCKS_CONNECT));
}

// Tests the case when the server sends a smaller sized handshake data
// and closes the connection.
TEST_F(SOCKSClientSocketTest, FailedSocketRead) {
  MockWrite data_writes[] = {MockWrite(ASYNC, kSOCKS4OkRequestLocalHostPort80,
                                       kSOCKS4OkRequestLocalHostPort80Length)};
  MockRead data_reads[] = {
      MockRead(ASYNC, kSOCKS4OkReply, kSOCKS4OkReplyLength - 2),
      // close connection unexpectedly
      MockRead(SYNCHRONOUS, 0)};
  RecordingNetLogObserver log_observer;

  user_sock_ = BuildMockSocket(data_reads, data_writes, host_resolver_.get(),
                               "localhost", 80, NetLog::Get());

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  auto entries = log_observer.GetEntries();
  EXPECT_TRUE(
      LogContainsBeginEvent(entries, 0, NetLogEventType::SOCKS_CONNECT));

  rv = callback_.WaitForResult();
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_CLOSED));
  EXPECT_FALSE(user_sock_->IsConnected());
  entries = log_observer.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SOCKS_CONNECT));
}

// Tries to connect to an unknown hostname. Should fail rather than
// falling back to SOCKS4a.
TEST_F(SOCKSClientSocketTest, FailedDNS) {
  const char hostname[] = "unresolved.ipv4.address";

  host_resolver_->rules()->AddSimulatedTimeoutFailure(hostname);

  RecordingNetLogObserver log_observer;

  user_sock_ =
      BuildMockSocket(base::span<MockRead>(), base::span<MockWrite>(),
                      host_resolver_.get(), hostname, 80, NetLog::Get());

  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  auto entries = log_observer.GetEntries();
  EXPECT_TRUE(
      LogContainsBeginEvent(entries, 0, NetLogEventType::SOCKS_CONNECT));

  rv = callback_.WaitForResult();
  EXPECT_THAT(rv, IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(user_sock_->GetResolveErrorInfo().error,
              IsError(ERR_DNS_TIMED_OUT));
  EXPECT_FALSE(user_sock_->IsConnected());
  entries = log_observer.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SOCKS_CONNECT));
}

// Calls Disconnect() while a host resolve is in progress. The outstanding host
// resolve should be cancelled.
TEST_F(SOCKSClientSocketTest, DisconnectWhileHostResolveInProgress) {
  auto hanging_resolver = std::make_unique<HangingHostResolver>();

  // Doesn't matter what the socket data is, we will never use it -- garbage.
  MockWrite data_writes[] = { MockWrite(SYNCHRONOUS, "", 0) };
  MockRead data_reads[] = { MockRead(SYNCHRONOUS, "", 0) };

  user_sock_ = BuildMockSocket(data_reads, data_writes, hanging_resolver.get(),
                               "foo", 80, nullptr);

  // Start connecting (will get stuck waiting for the host to resolve).
  int rv = user_sock_->Connect(callback_.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  EXPECT_FALSE(user_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnectedAndIdle());

  // Disconnect the SOCKS socket -- this should cancel the outstanding resolve.
  ASSERT_EQ(0, hanging_resolver->num_cancellations());
  user_sock_->Disconnect();
  EXPECT_EQ(1, hanging_resolver->num_cancellations());

  EXPECT_FALSE(user_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnectedAndIdle());

  // Need to delete `user_sock_` before the HostResolver it references.
  tcp_sock_ = nullptr;
  user_sock_.reset();
}

// Tries to connect to an IPv6 IP.  Should fail, as SOCKS4 does not support
// IPv6.
TEST_F(SOCKSClientSocketTest, NoIPv6) {
  const char kHostName[] = "::1";

  user_sock_ = BuildMockSocket(base::span<MockRead>(), base::span<MockWrite>(),
                               host_resolver_.get(), kHostName, 80, nullptr);

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED,
            callback_.GetResult(user_sock_->Connect(callback_.callback())));
}

// Same as above, but with a real resolver, to protect against regressions.
TEST_F(SOCKSClientSocketTest, NoIPv6RealResolver) {
  const char kHostName[] = "::1";

  std::unique_ptr<HostResolver> host_resolver(
      HostResolver::CreateStandaloneResolver(nullptr));

  user_sock_ = BuildMockSocket(base::span<MockRead>(), base::span<MockWrite>(),
                               host_resolver.get(), kHostName, 80, nullptr);

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED,
            callback_.GetResult(user_sock_->Connect(callback_.callback())));

  // Need to delete `user_sock_` before the HostResolver it references.
  tcp_sock_ = nullptr;
  user_sock_.reset();
}

TEST_F(SOCKSClientSocketTest, Tag) {
  StaticSocketDataProvider data;
  auto tagging_sock = std::make_unique<MockTaggingStreamSocket>(
      std::make_unique<MockTCPClientSocket>(address_list_, NetLog::Get(),
                                            &data));
  auto* tagging_sock_ptr = tagging_sock.get();

  auto connection = std::make_unique<ClientSocketHandle>();
  // |connection| takes ownership of |tagging_sock|, but keep a
  // non-owning pointer to it.
  MockHostResolver host_resolver;
  SOCKSClientSocket socket(
      std::move(tagging_sock), HostPortPair("localhost", 80),
      NetworkAnonymizationKey(), DEFAULT_PRIORITY, &host_resolver,
      SecureDnsPolicy::kAllow, TRAFFIC_ANNOTATION_FOR_TESTS);

  EXPECT_EQ(tagging_sock_ptr->tag(), SocketTag());
#if BUILDFLAG(IS_ANDROID)
  SocketTag tag(0x12345678, 0x87654321);
  socket.ApplySocketTag(tag);
  EXPECT_EQ(tagging_sock_ptr->tag(), tag);
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(SOCKSClientSocketTest, SetSecureDnsPolicy) {
  for (auto secure_dns_policy :
       {SecureDnsPolicy::kAllow, SecureDnsPolicy::kDisable}) {
    StaticSocketDataProvider data;
    MockHostResolver host_resolver;
    host_resolver.rules()->AddRule("doh.test", "127.0.0.1");
    SOCKSClientSocket socket(std::make_unique<MockTCPClientSocket>(
                                 address_list_, NetLog::Get(), &data),
                             HostPortPair("doh.test", 80),
                             NetworkAnonymizationKey(), DEFAULT_PRIORITY,
                             &host_resolver, secure_dns_policy,
                             TRAFFIC_ANNOTATION_FOR_TESTS);

    EXPECT_EQ(ERR_IO_PENDING, socket.Connect(callback_.callback()));
    EXPECT_EQ(secure_dns_policy, host_resolver.last_secure_dns_policy());
  }
}

}  // namespace net
