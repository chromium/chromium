// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#endif  // BUILDFLAG(IS_ANDROID)

// For getsockopt() call.
#if BUILDFLAG(IS_WIN)
#include <winsock2.h>

#include "net/socket/tcp_socket_io_completion_port_win.h"
#else  // !BUILDFLAG(IS_WIN)
#include <sys/socket.h>
#endif  //  !BUILDFLAG(IS_WIN)

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

// IOBuffer with the ability to invoke a callback when destroyed. Useful for
// checking for leaks.
class IOBufferWithDestructionCallback : public IOBufferWithSize {
 public:
  explicit IOBufferWithDestructionCallback(base::OnceClosure on_destroy_closure)
      : IOBufferWithSize(1024),
        on_destroy_closure_(std::move(on_destroy_closure)) {
    DCHECK(on_destroy_closure_);
  }

 protected:
  ~IOBufferWithDestructionCallback() override {
    std::move(on_destroy_closure_).Run();
  }

  base::OnceClosure on_destroy_closure_;
};

class TestSocketPerformanceWatcher : public SocketPerformanceWatcher {
 public:
  explicit TestSocketPerformanceWatcher(bool should_notify_updated_rtt)
      : should_notify_updated_rtt_(should_notify_updated_rtt) {}

  TestSocketPerformanceWatcher(const TestSocketPerformanceWatcher&) = delete;
  TestSocketPerformanceWatcher& operator=(const TestSocketPerformanceWatcher&) =
      delete;

  ~TestSocketPerformanceWatcher() override = default;

  bool ShouldNotifyUpdatedRTT() const override {
    return should_notify_updated_rtt_;
  }

  void OnUpdatedRTTAvailable(const base::TimeDelta& rtt) override {
    rtt_notification_count_++;
  }

  void OnConnectionChanged() override { connection_changed_count_++; }

  size_t rtt_notification_count() const { return rtt_notification_count_; }

  size_t connection_changed_count() const { return connection_changed_count_; }

 private:
  const bool should_notify_updated_rtt_;
  size_t connection_changed_count_ = 0u;
  size_t rtt_notification_count_ = 0u;
};

const int kListenBacklog = 5;

class TCPSocketTest
    : public PlatformTest,
      public WithTaskEnvironment,
      // The params indicate whether the
      // "TcpSocketIoCompletionPortWin" feature is enabled and
      // whether we should use Read() or ReadIfReady() to read
      // the data.
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  TCPSocketTest() {
#if BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitWithFeatureState(
        features::kTcpSocketIoCompletionPortWin,
        IsTcpSocketIoCompletionPortWinEnabled());
#else
    CHECK(!std::get<0>(GetParam()));
#endif  // BUILDFLAG(IS_WIN)
    socket_ = TCPSocket::Create(nullptr, nullptr, NetLogSource());
  }

#if BUILDFLAG(IS_WIN)
  bool IsTcpSocketIoCompletionPortWinEnabled() {
    return std::get<0>(GetParam());
  }
#endif  // BUILDFLAG(IS_WIN)

  bool ShouldUseReadIfReady() { return std::get<1>(GetParam()); }

  void SetUpListenIPv4() {
    ASSERT_THAT(socket_->Open(ADDRESS_FAMILY_IPV4), IsOk());
    ASSERT_THAT(socket_->Bind(IPEndPoint(IPAddress::IPv4Localhost(), 0)),
                IsOk());
    ASSERT_THAT(socket_->Listen(kListenBacklog), IsOk());
    ASSERT_THAT(socket_->GetLocalAddress(&local_address_), IsOk());
  }

  void SetUpListenIPv6(bool* success) {
    *success = false;

    if (socket_->Open(ADDRESS_FAMILY_IPV6) != OK ||
        socket_->Bind(IPEndPoint(IPAddress::IPv6Localhost(), 0)) != OK ||
        socket_->Listen(kListenBacklog) != OK) {
      LOG(ERROR) << "Failed to listen on ::1 - probably because IPv6 is "
          "disabled. Skipping the test";
      return;
    }
    ASSERT_THAT(socket_->GetLocalAddress(&local_address_), IsOk());
    *success = true;
  }

  std::pair<std::unique_ptr<TCPSocket>, std::unique_ptr<TCPSocket>>
  CreateIPv4SocketPair() {
    TestCompletionCallback connect_callback;
    std::unique_ptr<TCPSocket> connecting_socket =
        TCPSocket::Create(nullptr, nullptr, NetLogSource());
    int result = connecting_socket->Open(ADDRESS_FAMILY_IPV4);
    EXPECT_THAT(result, IsOk());
    int connect_result =
        connecting_socket->Connect(local_address_, connect_callback.callback());

    TestCompletionCallback accept_callback;
    std::unique_ptr<TCPSocket> accepted_socket;
    IPEndPoint accepted_address;
    result = socket_->Accept(&accepted_socket, &accepted_address,
                             accept_callback.callback());
    EXPECT_THAT(accept_callback.GetResult(result), IsOk());
    CHECK(accepted_socket.get());
    EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

    // Both sockets should be on the loopback network interface.
    EXPECT_EQ(accepted_address.address(), local_address_.address());

    return std::make_pair(std::move(connecting_socket),
                          std::move(accepted_socket));
  }

  void TestAcceptAsync() {
    TestCompletionCallback accept_callback;
    std::unique_ptr<TCPSocket> accepted_socket;
    IPEndPoint accepted_address;
    ASSERT_THAT(socket_->Accept(&accepted_socket, &accepted_address,
                                accept_callback.callback()),
                IsError(ERR_IO_PENDING));

    TestCompletionCallback connect_callback;
    TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                      nullptr, NetLogSource());
    int connect_result = connecting_socket.Connect(connect_callback.callback());
    EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

    EXPECT_THAT(accept_callback.WaitForResult(), IsOk());

    EXPECT_TRUE(accepted_socket.get());

    // Both sockets should be on the loopback network interface.
    EXPECT_EQ(accepted_address.address(), local_address_.address());
  }

#if defined(TCP_INFO) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Tests that notifications to Socket Performance Watcher (SPW) are delivered
  // correctly. |should_notify_updated_rtt| is true if the SPW is interested in
  // receiving RTT notifications. |num_messages| is the number of messages that
  // are written/read by the sockets. |expect_connection_changed_count| is the
  // expected number of connection change notifications received by the SPW.
  // |expect_rtt_notification_count| is the expected number of RTT
  // notifications received by the SPW. This test works by writing
  // |num_messages| to the socket. A different socket (with a SPW attached to
  // it) reads the messages.
  void TestSPWNotifications(bool should_notify_updated_rtt,
                            size_t num_messages,
                            size_t expect_connection_changed_count,
                            size_t expect_rtt_notification_count) {
    ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

    TestCompletionCallback connect_callback;

    auto watcher = std::make_unique<TestSocketPerformanceWatcher>(
        should_notify_updated_rtt);
    TestSocketPerformanceWatcher* watcher_ptr = watcher.get();

    std::unique_ptr<TCPSocket> connecting_socket =
        TCPSocket::Create(std::move(watcher), nullptr, NetLogSource());

    int result = connecting_socket->Open(ADDRESS_FAMILY_IPV4);
    ASSERT_THAT(result, IsOk());
    int connect_result =
        connecting_socket->Connect(local_address_, connect_callback.callback());

    TestCompletionCallback accept_callback;
    std::unique_ptr<TCPSocket> accepted_socket;
    IPEndPoint accepted_address;
    result = socket_->Accept(&accepted_socket, &accepted_address,
                             accept_callback.callback());
    ASSERT_THAT(accept_callback.GetResult(result), IsOk());

    ASSERT_TRUE(accepted_socket.get());

    // Both sockets should be on the loopback network interface.
    EXPECT_EQ(accepted_address.address(), local_address_.address());

    ASSERT_THAT(connect_callback.GetResult(connect_result), IsOk());

    for (size_t i = 0; i < num_messages; ++i) {
      // Use a 1 byte message so that the watcher is notified at most once per
      // message.
      static constexpr std::string_view message = "t";

      auto write_buffer =
          base::MakeRefCounted<VectorIOBuffer>(base::as_byte_span(message));

      TestCompletionCallback write_callback;
      int write_result = accepted_socket->Write(
          write_buffer.get(), write_buffer->size(), write_callback.callback(),
          TRAFFIC_ANNOTATION_FOR_TESTS);

      scoped_refptr<IOBufferWithSize> read_buffer =
          base::MakeRefCounted<IOBufferWithSize>(message.size());
      TestCompletionCallback read_callback;
      int read_result = connecting_socket->Read(
          read_buffer.get(), read_buffer->size(), read_callback.callback());

      ASSERT_EQ(1, write_callback.GetResult(write_result));
      ASSERT_EQ(1, read_callback.GetResult(read_result));
    }
    EXPECT_EQ(expect_connection_changed_count,
              watcher_ptr->connection_changed_count());
    EXPECT_EQ(expect_rtt_notification_count,
              watcher_ptr->rtt_notification_count());
  }
#endif  // defined(TCP_INFO) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  AddressList local_address_list() const {
    return AddressList(local_address_);
  }

  // Helper function to read `expected_size` bytes from the accepting
  // socket using ReadIfReady(). Data will be temporarily read into
  // `read_buffer` and then accumulated in `received_data_buffer`. On
  // successful return, `received_data` will be modified to refer to
  // the read data. On failure, `received_data` will be unmodified.
  // Correctly handles cases where data is split across multiple
  // ReadIfReady() calls.
  void ReadAllAvailableData(TCPSocket* connecting_socket,
                            TCPSocket* accepted_socket,
                            IOBufferWithSize* read_buffer,
                            base::span<uint8_t> received_data_buffer,
                            size_t expected_size,
                            base::span<const uint8_t>* received_data) {
    ASSERT_LE(expected_size, received_data_buffer.size());
    size_t total_received = 0;
    while (total_received < expected_size) {
      EXPECT_TRUE(connecting_socket->IsConnected());
      EXPECT_TRUE(accepted_socket->IsConnected());

      TestCompletionCallback read_callback;
      int read_result = connecting_socket->ReadIfReady(
          read_buffer, read_buffer->size(), read_callback.callback());

      if (read_result == ERR_IO_PENDING) {
        read_result = read_callback.GetResult(read_result);
        ASSERT_EQ(read_result, 0);
        read_result = connecting_socket->ReadIfReady(
            read_buffer, read_buffer->size(), read_callback.callback());
        ASSERT_GT(read_result, 0);
        DVLOG(1) << "Got data in while loop. Size " << read_result;
      }

      if (read_result > 0) {
        ASSERT_LE(total_received + read_result, expected_size);
        received_data_buffer.subspan(total_received)
            .copy_prefix_from(
                read_buffer->first(static_cast<size_t>(read_result)));

        total_received += read_result;
        DVLOG(1) << "Copied data in while loop. Size " << total_received;
      } else {
        FAIL() << "**** Failed to read data using ReadIfReady(). Return: "
               << read_result << " connecting socket connected "
               << connecting_socket->IsConnected()
               << " Accepting socket connected "
               << accepted_socket->IsConnected();
      }
    }
    *received_data = received_data_buffer.first(total_received);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TCPSocket> socket_;
  IPEndPoint local_address_;
};

// Test listening and accepting with a socket bound to an IPv4 address.
TEST_P(TCPSocketTest, Accept) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback connect_callback;
  // TODO(yzshen): Switch to use TCPSocket when it supports client socket
  // operations.
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  int result = socket_->Accept(&accepted_socket, &accepted_address,
                               accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  EXPECT_TRUE(accepted_socket.get());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(accepted_address.address(), local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

// Test Accept() callback.
TEST_P(TCPSocketTest, AcceptAsync) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  TestAcceptAsync();
}

// Test AdoptConnectedSocket()
TEST_P(TCPSocketTest, AdoptConnectedSocket) {
  std::unique_ptr<TCPSocket> accepting_socket =
      TCPSocket::Create(nullptr, nullptr, NetLogSource());
  ASSERT_THAT(accepting_socket->Open(ADDRESS_FAMILY_IPV4), IsOk());
  ASSERT_THAT(accepting_socket->Bind(IPEndPoint(IPAddress::IPv4Localhost(), 0)),
              IsOk());
  ASSERT_THAT(accepting_socket->GetLocalAddress(&local_address_), IsOk());
  ASSERT_THAT(accepting_socket->Listen(kListenBacklog), IsOk());

  TestCompletionCallback connect_callback;
  // TODO(yzshen): Switch to use TCPSocket when it supports client socket
  // operations.
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  int result = accepting_socket->Accept(&accepted_socket, &accepted_address,
                                        accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  SocketDescriptor accepted_descriptor =
      accepted_socket->ReleaseSocketDescriptorForTesting();

  ASSERT_THAT(
      socket_->AdoptConnectedSocket(accepted_descriptor, accepted_address),
      IsOk());

  // socket_ should now have the local address.
  IPEndPoint adopted_address;
  ASSERT_THAT(socket_->GetLocalAddress(&adopted_address), IsOk());
  EXPECT_EQ(local_address_.address(), adopted_address.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

// Test Accept() for AdoptUnconnectedSocket.
TEST_P(TCPSocketTest, AcceptForAdoptedUnconnectedSocket) {
  SocketDescriptor existing_socket =
      CreatePlatformSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_THAT(socket_->AdoptUnconnectedSocket(existing_socket), IsOk());

  IPEndPoint address(IPAddress::IPv4Localhost(), 0);
  SockaddrStorage storage;
  ASSERT_TRUE(address.ToSockAddr(storage.addr(), &storage.addr_len));
  ASSERT_EQ(0, bind(existing_socket, storage.addr(), storage.addr_len));

  ASSERT_THAT(socket_->Listen(kListenBacklog), IsOk());
  ASSERT_THAT(socket_->GetLocalAddress(&local_address_), IsOk());

  TestAcceptAsync();
}

// Accept two connections simultaneously.
TEST_P(TCPSocketTest, Accept2Connections) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;

  ASSERT_THAT(socket_->Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback connect_callback2;
  TCPClientSocket connecting_socket2(local_address_list(), nullptr, nullptr,
                                     nullptr, NetLogSource());
  int connect_result2 =
      connecting_socket2.Connect(connect_callback2.callback());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());

  TestCompletionCallback accept_callback2;
  std::unique_ptr<TCPSocket> accepted_socket2;
  IPEndPoint accepted_address2;

  int result = socket_->Accept(&accepted_socket2, &accepted_address2,
                               accept_callback2.callback());
  ASSERT_THAT(accept_callback2.GetResult(result), IsOk());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
  EXPECT_THAT(connect_callback2.GetResult(connect_result2), IsOk());

  EXPECT_TRUE(accepted_socket.get());
  EXPECT_TRUE(accepted_socket2.get());
  EXPECT_NE(accepted_socket.get(), accepted_socket2.get());

  EXPECT_EQ(accepted_address.address(), local_address_.address());
  EXPECT_EQ(accepted_address2.address(), local_address_.address());
}

// Test listening and accepting with a socket bound to an IPv6 address.
TEST_P(TCPSocketTest, AcceptIPv6) {
  bool initialized = false;
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv6(&initialized));
  if (!initialized)
    return;

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  int result = socket_->Accept(&accepted_socket, &accepted_address,
                               accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  EXPECT_TRUE(accepted_socket.get());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(accepted_address.address(), local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

namespace {

void TestReadWrite(std::unique_ptr<TCPSocket> socket1,
                   std::unique_ptr<TCPSocket> socket2) {
  const std::string message("test message");
  const auto drainable_write_buffer = base::MakeRefCounted<DrainableIOBuffer>(
      base::MakeRefCounted<StringIOBuffer>(message), message.size());

  while (drainable_write_buffer->BytesRemaining() > 0) {
    TestCompletionCallback write_callback;
    int write_result = socket1->Write(
        drainable_write_buffer.get(), drainable_write_buffer->BytesRemaining(),
        write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    write_result = write_callback.GetResult(write_result);
    ASSERT_GE(write_result, 0);
    ASSERT_LE(write_result, drainable_write_buffer->BytesRemaining());
    drainable_write_buffer->DidConsume(write_result);
  }

  const auto read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(message.size());
  const auto drainable_read_buffer =
      base::MakeRefCounted<DrainableIOBuffer>(read_buffer, read_buffer->size());

  while (drainable_read_buffer->BytesRemaining() > 0) {
    TestCompletionCallback read_callback;
    int read_result = socket2->Read(drainable_read_buffer.get(),
                                    drainable_read_buffer->BytesRemaining(),
                                    read_callback.callback());
    read_result = read_callback.GetResult(read_result);
    ASSERT_GE(read_result, 0);
    ASSERT_LE(read_result, drainable_read_buffer->BytesRemaining());
    drainable_read_buffer->DidConsume(read_result);
  }

  const std::string received_message(read_buffer->data(), read_buffer->size());
  EXPECT_EQ(message, received_message);
}

}  // namespace

TEST_P(TCPSocketTest, ReadWrite) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [socket1, socket2] = CreateIPv4SocketPair();
  TestReadWrite(std::move(socket1), std::move(socket2));
}

#if BUILDFLAG(IS_WIN)
// Same test as above, but exercises the code used when
// `FILE_SKIP_COMPLETION_PORT_ON_SUCCESS` is not supported.
TEST_P(TCPSocketTest, ReadWriteNoSkipCompletionPortOnSuccess) {
  if (!IsTcpSocketIoCompletionPortWinEnabled()) {
    // FILE_SKIP_COMPLETION_PORT_ON_SUCCESS is only used by
    // `TcpSocketIoCompletionPortWin`.
    return;
  }

  TcpSocketIoCompletionPortWin::DisableSkipCompletionPortOnSuccessForTesting
      disable_skip_completion_port_on_success;

  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [socket1, socket2] = CreateIPv4SocketPair();
  TestReadWrite(std::move(socket1), std::move(socket2));
}
#endif  // BUILDFLAG(IS_WIN)

// Destroy a TCPSocket while there's a pending read, and make sure the read
// IOBuffer that the socket was holding on to is destroyed.
// See https://crbug.com/804868.
TEST_P(TCPSocketTest, DestroyWithPendingRead) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  // Try to read from the socket, but never write anything to the other end.
  base::RunLoop run_loop;
  scoped_refptr<IOBufferWithDestructionCallback> read_buffer(
      base::MakeRefCounted<IOBufferWithDestructionCallback>(
          run_loop.QuitClosure()));
  TestCompletionCallback read_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            connecting_socket->Read(read_buffer.get(), read_buffer->size(),
                                    read_callback.callback()));

  // Release the handle to the read buffer and destroy the socket. Make sure the
  // read buffer is destroyed.
  read_buffer = nullptr;
  connecting_socket.reset();
  run_loop.Run();
}

// Destroy a TCPSocket while there's a pending write, and make sure the write
// IOBuffer that the socket was holding on to is destroyed.
TEST_P(TCPSocketTest, DestroyWithPendingWrite) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  // Repeatedly write to the socket until an operation does not complete
  // synchronously.
  base::RunLoop run_loop;
  scoped_refptr<IOBufferWithDestructionCallback> write_buffer(
      base::MakeRefCounted<IOBufferWithDestructionCallback>(
          run_loop.QuitClosure()));
  std::ranges::fill(write_buffer->span(), '1');
  TestCompletionCallback write_callback;
  while (true) {
    int result = connecting_socket->Write(
        write_buffer.get(), write_buffer->size(), write_callback.callback(),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result == ERR_IO_PENDING)
      break;
    ASSERT_LT(0, result);
  }

  // Release the handle to the read buffer and destroy the socket. Make sure the
  // write buffer is destroyed.
  write_buffer = nullptr;
  connecting_socket.reset();
  run_loop.Run();
}

// If a ReadIfReady is pending, it's legal to cancel it and start reading later.
TEST_P(TCPSocketTest, CancelPendingReadIfReady) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  // Try to read from the socket, but never write anything to the other end.
  base::RunLoop run_loop;
  scoped_refptr<IOBufferWithDestructionCallback> read_buffer(
      base::MakeRefCounted<IOBufferWithDestructionCallback>(
          run_loop.QuitClosure()));
  TestCompletionCallback read_callback;

  int read_if_ready_result = connecting_socket->ReadIfReady(
      read_buffer.get(), read_buffer->size(), read_callback.callback());

  EXPECT_EQ(ERR_IO_PENDING, read_if_ready_result);

  // Now cancel the pending ReadIfReady().
  ASSERT_EQ(OK, connecting_socket->CancelReadIfReady());

  // Send data to |connecting_socket|.
  static constexpr base::cstring_view kMsg = "hello!";

  scoped_refptr<IOBufferWithSize> write_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kMsg.size());
  write_buffer->span().copy_from(base::as_byte_span(kMsg));

  TestCompletionCallback write_callback;
  int write_result = accepted_socket->Write(write_buffer.get(), kMsg.size(),
                                            write_callback.callback(),
                                            TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(static_cast<int>(kMsg.size()), write_result);

  TestCompletionCallback read_callback2;
  int read_result = connecting_socket->ReadIfReady(
      read_buffer.get(), read_buffer->size(), read_callback2.callback());
  if (read_result == ERR_IO_PENDING) {
    ASSERT_EQ(OK, read_callback2.GetResult(read_result));
    read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback2.callback());
  }

  ASSERT_EQ(static_cast<int>(kMsg.size()), read_result);
  ASSERT_EQ(read_buffer->first(static_cast<size_t>(read_result)),
            base::as_byte_span(kMsg));
}

// Validates that we can successfully read data from the underlying socket after
// cancelling a pending read request when data is available and issuing a new
// ReadIfReady()/Read() request.
TEST_P(TCPSocketTest, CancelReadRetainedBufferedData) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  connecting_socket->SetDefaultOptionsForClient();
  accepted_socket->SetDefaultOptionsForClient();

  // The test setup is as below.
  // 1. First issue a ReadIfReady() call on the connecting socket. This will
  //    return ERR_IO_PENDING.
  // 2. Write data from the accepted socket (other side).
  // 3. Cancel the first read.
  // 4. Write more data from the accepted socket.
  // 5. Attempt to read the data via ReadIfReady().

  std::array<uint8_t, 1024> received_data = {};
  size_t received_data_size = 0;
  size_t bytes_written = 0;

  // Set up a read buffer and initiate a read.
  scoped_refptr<IOBufferWithSize> read_buffer1 =
      base::MakeRefCounted<IOBufferWithSize>(1024);
  TestCompletionCallback read_callback1;

  int read_result = connecting_socket->ReadIfReady(
      read_buffer1.get(), read_buffer1->size(), read_callback1.callback());

  ASSERT_EQ(ERR_IO_PENDING, read_result);

  // Write data to the socket before cancelling the read operation.
  constexpr std::string_view kMsg = "test_data.Part 1";
  scoped_refptr<IOBufferWithSize> write_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kMsg.size());
  write_buffer->span().copy_from(base::as_byte_span(kMsg));

  TestCompletionCallback write_callback;
  int write_result1 = accepted_socket->Write(write_buffer.get(), kMsg.size(),
                                             write_callback.callback(),
                                             TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(static_cast<int>(kMsg.size()),
            write_callback.GetResult(write_result1));
  bytes_written += kMsg.size();

  // Cancel the pending read operation.
  connecting_socket->CancelReadIfReady();

  // Handle the case where the ReadIfRead() operation completed before the
  // CancelReadIfReady call.
  if (read_callback1.have_result()) {
    EXPECT_EQ(net::OK, read_callback1.GetResult(read_result));
    DVLOG(1) << "Got data after CancelReadIfReady() " << received_data_size;
  }

  constexpr std::string_view kMsg2 = "test_data. Part 2";
  scoped_refptr<IOBufferWithSize> write_buffer2 =
      base::MakeRefCounted<IOBufferWithSize>(kMsg2.size());
  write_buffer2->span().copy_from(base::as_byte_span(kMsg2));

  TestCompletionCallback write_callback2;
  int write_result2 = accepted_socket->Write(write_buffer2.get(), kMsg2.size(),
                                             write_callback2.callback(),
                                             TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(static_cast<int>(kMsg2.size()),
            write_callback2.GetResult(write_result2));
  bytes_written += kMsg2.size();

  // Set up a new buffer and initiate a read.
  scoped_refptr<IOBufferWithSize> read_buffer2 =
      base::MakeRefCounted<IOBufferWithSize>(1024);

  // The data read off the socket is copied into this buffer.
  base::span<const uint8_t> received_data_buffer;

  // Use `span` for `received_data` directly.
  ReadAllAvailableData(connecting_socket.get(), accepted_socket.get(),
                       read_buffer2.get(), received_data, bytes_written,
                       &received_data_buffer);
  received_data_size = received_data_buffer.size();

  // Validate that the read operation successfully retrieved the expected data.
  ASSERT_EQ(bytes_written, received_data_size);
  std::string expected_data = std::string(kMsg) + std::string(kMsg2);
  ASSERT_EQ(base::as_bytes(base::span(expected_data)),
            received_data_buffer.first(received_data_size));
}

// Validates behavior when a pending ReadIfRead() is canceled and the socket
// remains active for further operations. This test ensures that data written to
// the socket before and after a cancel operation is read successfully by
// subsequent ReadIfReady() requests.
//
// The test setup is as follows:
// 1. Issue an initial ReadIfReady on the connecting socket.
//    This will return ERR_IO_PENDING as there is no data available initially.
// 2. Write data from the accepted socket (peer).
// 3. Call CancelReadIfReady() on the connecting socket to cancel the pending
//    read operation. If the read callback has already run, it captures the
//    available data.
// 4. Write additional data from the accepted socket.
// 5. Issue subsequent read requests to ensure that all data written to the
//    socket is received correctly.
//
// Special Considerations:
// - For ReadIfReady, after the callback is invoked, the test ensures that
//   another ReadIfReady call is made to retrieve the available data.
//
TEST_P(TCPSocketTest, CancelThenImmediateReadIfReady) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  connecting_socket->SetDefaultOptionsForClient();
  accepted_socket->SetDefaultOptionsForClient();

  // Set up a read buffer and initiate a ReadIfReady or Read.
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(1024);
  TestCompletionCallback read_callback;

  int read_result = connecting_socket->ReadIfReady(
      read_buffer.get(), read_buffer->size(), read_callback.callback());

  ASSERT_EQ(ERR_IO_PENDING, read_result);

  // Write data to the socket.
  static constexpr base::cstring_view kMsg = "test_data";

  scoped_refptr<IOBufferWithSize> write_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kMsg.size());
  write_buffer->span().copy_from(base::as_byte_span(kMsg));

  TestCompletionCallback write_callback;
  ASSERT_EQ(static_cast<int>(kMsg.size()),
            accepted_socket->Write(write_buffer.get(), kMsg.size(),
                                   write_callback.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));
  // Small sleep to allow the write to be propagated. In some environments, e.g
  // IOS simulator, Fuchsia, we have seen cases where the write does not
  // propagate through even though the Write call returns that data was written
  // to the socket. The test relies on the CancelReadIfReady() call executing
  // just around the time the data becomes available for read.
  //
  // We may need to rewrite this test in the event it becomes flaky.
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  // Cancel the ReadIfReady() operation and immediately issue another
  // ReadIfReady().
  connecting_socket->CancelReadIfReady();

  std::array<uint8_t, 1024> received_data = {};
  // The data read off the socket is copied into this buffer.
  base::span<const uint8_t> received_data_buffer;
  // Read may not return the entire data in one call. Handle the case where
  // we may get partial data.
  ReadAllAvailableData(connecting_socket.get(), accepted_socket.get(),
                       read_buffer.get(), received_data, kMsg.size(),
                       &received_data_buffer);

  // Validate that the data was read correctly.
  ASSERT_EQ(kMsg.size(), received_data_buffer.size());
  ASSERT_EQ(base::as_byte_span(kMsg),
            received_data_buffer.first(received_data_buffer.size()));
}

// Validates that large amounts of data can be read correctly in multiple
// chunks with interspersed CancelReadIfReady() calls This test ensures that
// when a large payload is written to the socket, the reading side can retrieve
// the data in several smaller read operations without data loss or corruption.
// The test verifies both Read and ReadIfReady behavior when handling large data
// transfers with the Read/ReadIfReady() calls cancelled periodically.
//
// The test setup is as follows:
// 1. Generate a large data payload (e.g., 10 KB).
// 2. Write the payload from the accepted socket (peer) to the connecting
//    socket in 2K chunks.
// 3. On the connecting socket, repeatedly call ReadIfReady or Read with the
//    read chunk size (1KB) to read the data.
// 4. Issue CancelReadIfReady() calls after every 2K chunk written.
// 5. Continue reading until all data has been received.
// 6. Verify that the concatenated received data matches the original payload.
//
// Special Considerations:
// - For ReadIfReady, after each callback indicating data readiness, the test
//   issues another ReadIfReady call to retrieve the next chunk.
TEST_P(TCPSocketTest, LargeDataReadWithCancelReadIfReady) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  connecting_socket->SetDefaultOptionsForClient();
  accepted_socket->SetDefaultOptionsForClient();

  // Generate a large data payload (10 KB of 'x').
  constexpr size_t chunk_size = 2048;       // Write in 2 KB chunks.
  constexpr size_t read_chunk_size = 1024;  // Read in 1 KB chunks.
  constexpr size_t total_size = 10 * 1024;  // 10 KB.
  std::vector<uint8_t> large_data(total_size, 0u);
  base::RandBytes(large_data);

  std::vector<uint8_t> received_data(total_size, 0);
  size_t received_data_size = 0;

  // Iterate over chunks to write and read in parts.
  for (size_t offset = 0; offset < total_size; offset += chunk_size) {
    // Determine the size of the current chunk.
    size_t current_chunk_size = std::min(chunk_size, total_size - offset);

    scoped_refptr<IOBufferWithSize> write_buffer =
        base::MakeRefCounted<IOBufferWithSize>(current_chunk_size);

    // Copy the data to be sent out into the write buffer.
    write_buffer->span()
        .first(current_chunk_size)
        .copy_from(
            base::as_byte_span(large_data).subspan(offset, current_chunk_size));

    // Issue a read before writing the chunk.
    scoped_refptr<IOBufferWithSize> read_buffer =
        base::MakeRefCounted<IOBufferWithSize>(read_chunk_size);

    TestCompletionCallback read_callback;
    int read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback.callback());

    ASSERT_EQ(ERR_IO_PENDING, read_result);

    // Write the chunk to the socket.
    TestCompletionCallback write_callback;
    ASSERT_EQ(static_cast<int>(current_chunk_size),
              accepted_socket->Write(write_buffer.get(), current_chunk_size,
                                     write_callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS));

    // Small sleep to allow the write to be propagated.
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

    // Cancel the pending read above.
    connecting_socket->CancelReadIfReady();

    // The CancelReadIfReady() call above clears out the completion callback
    // passed in by the caller. This also means that the caller won't receive
    // a notification about available data in the socket after the
    // CancelReadIfReady() call. We have a small Run loop below to give
    // sufficient time for the IO completion to be signaled. Please note that
    // this only applies to the Windows (IO completion based socket). Other
    // implementations just stop listening for read completions.
    base::RunLoop run_loop;
    base::OneShotTimer timeout_timer;
    timeout_timer.Start(FROM_HERE, TestTimeouts::tiny_timeout(),
                        base::BindOnce(&base::RunLoop::QuitWhenIdle,
                                       base::Unretained(&run_loop)));
    run_loop.Run();

    // Read until the entire chunk is retrieved.
    size_t chunk_received = 0;
    while (chunk_received < current_chunk_size) {
      TestCompletionCallback retry_read_callback;
      read_result =
          connecting_socket->ReadIfReady(read_buffer.get(), read_buffer->size(),
                                         retry_read_callback.callback());
      if (read_result == ERR_IO_PENDING) {
        read_result = retry_read_callback.GetResult(read_result);
        ASSERT_GE(read_result, 0);
        read_result = connecting_socket->ReadIfReady(
            read_buffer.get(), read_buffer->size(),
            retry_read_callback.callback());
      }

      ASSERT_GT(read_result, 0);
      // Append received data to the buffer using spans.
      base::span<uint8_t>(received_data)
          .subspan(received_data_size, static_cast<size_t>(read_result))
          .copy_from(read_buffer->first(static_cast<size_t>(read_result)));
      received_data_size += read_result;
      chunk_received += read_result;
    }
  }
  // Validate that all the data was read correctly.
  ASSERT_EQ(base::span<const uint8_t>(received_data).first(received_data_size),
            base::as_bytes(base::span(large_data)));
}

TEST_P(TCPSocketTest, ReadComplete) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  // Send data to |connecting_socket|.
  static constexpr base::cstring_view kMsg = "hello!";

  scoped_refptr<StringIOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(std::string(kMsg));

  TestCompletionCallback write_callback;
  int write_result = accepted_socket->Write(write_buffer.get(), kMsg.size(),
                                            write_callback.callback(),
                                            TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(static_cast<int>(kMsg.size()), write_result);

  // ReadIfReady should read all of the data.
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kMsg.size());
  TestCompletionCallback read_callback;
  int read_result = 0;
  if (ShouldUseReadIfReady()) {
    read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    if (read_result == ERR_IO_PENDING) {
      ASSERT_EQ(OK, read_callback.GetResult(read_result));
      read_result = connecting_socket->ReadIfReady(
          read_buffer.get(), read_buffer->size(), read_callback.callback());
    }
  } else {
    read_result = connecting_socket->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    if (read_result == ERR_IO_PENDING) {
      // For read calls issuing GetResult is enough as the data will be copied.
      read_result = read_callback.GetResult(read_result);
      ASSERT_GE(read_result, 0);
    }
  }
  ASSERT_EQ(static_cast<int>(kMsg.size()), read_result);
  ASSERT_EQ(base::span(kMsg), read_buffer->span());
}

// This test is similar to ReadComplete, but with a larger buffer size than the
// available data.
TEST_P(TCPSocketTest, ReadBiggerRead) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  // Send data to |connecting_socket|.
  static constexpr base::cstring_view kMsg = "hello!";

  scoped_refptr<StringIOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(std::string(kMsg));
  TestCompletionCallback write_callback;
  int write_result = accepted_socket->Write(write_buffer.get(), kMsg.size(),
                                            write_callback.callback(),
                                            TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(static_cast<int>(kMsg.size()), write_result);

  // Read/ReadIfReady should read all of the data.
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kMsg.size() * 2);
  TestCompletionCallback read_callback;
  int read_result = 0;

  if (ShouldUseReadIfReady()) {
    read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    if (read_result == ERR_IO_PENDING) {
      ASSERT_EQ(OK, read_callback.GetResult(read_result));
      read_result = connecting_socket->ReadIfReady(
          read_buffer.get(), read_buffer->size(), read_callback.callback());
    }
  } else {
    read_result = connecting_socket->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    if (read_result == ERR_IO_PENDING) {
      // For read calls issuing GetResult is enough as the data will be copied.
      read_result = read_callback.GetResult(read_result);
      ASSERT_GE(read_result, 0);
    }
  }

  ASSERT_EQ(static_cast<int>(kMsg.size()), read_result);
  ASSERT_EQ(base::span(kMsg),
            read_buffer->first(static_cast<uint8_t>(read_result)));
}

// This test is similar to ReadComplete, but with a smaller buffer size than the
// available data.
TEST_P(TCPSocketTest, ReadSmallerRead) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  // Send data to |connecting_socket|.
  static constexpr base::cstring_view kMsg = "hello!";

  scoped_refptr<StringIOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(std::string(kMsg));

  TestCompletionCallback write_callback;
  int write_result = accepted_socket->Write(write_buffer.get(), kMsg.size(),
                                            write_callback.callback(),
                                            TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(static_cast<int>(kMsg.size()), write_result);

  // Read/ReadIfReady should read all of the data.
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kMsg.size() / 2);
  TestCompletionCallback read_callback;
  int read_result = 0;
  if (ShouldUseReadIfReady()) {
    read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    if (read_result == ERR_IO_PENDING) {
      ASSERT_EQ(OK, read_callback.GetResult(read_result));
      read_result = connecting_socket->ReadIfReady(
          read_buffer.get(), read_buffer->size(), read_callback.callback());
    }
  } else {
    read_result = connecting_socket->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    if (read_result == ERR_IO_PENDING) {
      // For read calls issuing GetResult is enough as the data will be copied.
      read_result = read_callback.GetResult(read_result);
      ASSERT_GE(read_result, 0);
    }
  }
  ASSERT_EQ(static_cast<int>(kMsg.size()) / 2, read_result);
  ASSERT_EQ(base::span(kMsg).first(static_cast<uint8_t>(read_result)),
            read_buffer->span());
}

TEST_P(TCPSocketTest, ReadNoDataThenClose) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  static constexpr base::cstring_view kMsg = "hello!";

  // Read/ReadIfReady should report that a read is pending if there's no data
  // yet.
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kMsg.size());
  TestCompletionCallback read_callback;
  int read_result = 0;

  if (ShouldUseReadIfReady()) {
    read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
  } else {
    read_result = connecting_socket->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
  }

  ASSERT_EQ(ERR_IO_PENDING, read_result);

  // Gracefully close the connection.
  accepted_socket->Close();
  accepted_socket.reset();

  read_result = read_callback.WaitForResult();
  ASSERT_EQ(0, read_result);
}

TEST_P(TCPSocketTest, ReadDataAfter) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [connecting_socket, accepted_socket] = CreateIPv4SocketPair();

  static constexpr base::cstring_view kMsg = "hello!";

  // ReadIfReady should report that a read is pending if there's no data yet.
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(kMsg.size());
  TestCompletionCallback read_callback;
  int read_result = 0;

  if (ShouldUseReadIfReady()) {
    read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
  } else {
    read_result = connecting_socket->Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
  }

  ASSERT_EQ(ERR_IO_PENDING, read_result);

  ASSERT_FALSE(read_callback.have_result());

  // Send data to |connecting_socket|.
  scoped_refptr<StringIOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(std::string(kMsg));
  TestCompletionCallback write_callback;
  int write_result = accepted_socket->Write(write_buffer.get(), kMsg.size(),
                                            write_callback.callback(),
                                            TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_EQ(static_cast<int>(kMsg.size()), write_result);

  read_result = read_callback.WaitForResult();
  if (ShouldUseReadIfReady()) {
    ASSERT_EQ(OK, read_result);
    // Now the data can be read.
    read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
  } else {
    // For read calls issuing GetResult is enough as the data will be copied.
    ASSERT_GE(read_result, 0);
  }

  ASSERT_EQ(static_cast<int>(kMsg.size()), read_result);
  ASSERT_EQ(base::span(kMsg), read_buffer->span());
}

TEST_P(TCPSocketTest, IsConnected) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  EXPECT_THAT(socket_->Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());

  // Immediately after creation, the socket should not be connected.
  EXPECT_FALSE(connecting_socket.IsConnected());
  EXPECT_FALSE(connecting_socket.IsConnectedAndIdle());

  int connect_result = connecting_socket.Connect(connect_callback.callback());
  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  // |connecting_socket| and |accepted_socket| should now both be reported as
  // connected, and idle
  EXPECT_TRUE(accepted_socket->IsConnected());
  EXPECT_TRUE(accepted_socket->IsConnectedAndIdle());
  EXPECT_TRUE(connecting_socket.IsConnected());
  EXPECT_TRUE(connecting_socket.IsConnectedAndIdle());

  // Write one byte to the |accepted_socket|, then close it.
  const char kSomeData[] = "!";
  scoped_refptr<IOBuffer> some_data_buffer =
      base::MakeRefCounted<StringIOBuffer>(kSomeData);
  TestCompletionCallback write_callback;
  EXPECT_THAT(write_callback.GetResult(accepted_socket->Write(
                  some_data_buffer.get(), 1, write_callback.callback(),
                  TRAFFIC_ANNOTATION_FOR_TESTS)),
              1);
  accepted_socket.reset();

  // Wait until |connecting_socket| is signalled as having data to read.
  fd_set read_fds;
  // SAFETY: The implementations are different on different platforms. However,
  // they are all operations on the read_fds object itself. No out-of-bounds
  // behavior will occur.
  UNSAFE_BUFFERS(FD_ZERO(&read_fds));
  SocketDescriptor connecting_fd =
      connecting_socket.SocketDescriptorForTesting();
  // SAFETY: There is an fd array in read_fds, which has different sizes on
  // different platforms, but is always greater than 1. It will record and check
  // the number of current fds internally. Since the memory layout and structure
  // of different platforms are inconsistent, we still use the system standard
  // interface.
  UNSAFE_BUFFERS(FD_SET(connecting_fd, &read_fds));
  ASSERT_EQ(select(FD_SETSIZE, &read_fds, nullptr, nullptr, nullptr), 1);
  // SAFETY: Check if this fd exists in read_fds. The system has already handled
  // the edge cases.
  ASSERT_TRUE(UNSAFE_BUFFERS(FD_ISSET(connecting_fd, &read_fds)));

  // It should now be reported as connected, but not as idle.
  EXPECT_TRUE(connecting_socket.IsConnected());
  EXPECT_FALSE(connecting_socket.IsConnectedAndIdle());

  // Read the message from |connecting_socket_|, then read the end-of-stream.
  scoped_refptr<IOBufferWithSize> read_buffer =
      base::MakeRefCounted<IOBufferWithSize>(2);
  TestCompletionCallback read_callback;
  EXPECT_THAT(
      read_callback.GetResult(connecting_socket.Read(
          read_buffer.get(), read_buffer->size(), read_callback.callback())),
      1);
  EXPECT_THAT(
      read_callback.GetResult(connecting_socket.Read(
          read_buffer.get(), read_buffer->size(), read_callback.callback())),
      0);

  // |connecting_socket| has no more data to read, so should noe be reported
  // as disconnected.
  EXPECT_FALSE(connecting_socket.IsConnected());
  EXPECT_FALSE(connecting_socket.IsConnectedAndIdle());
}

// Tests that setting a socket option in the BeforeConnectCallback works. With
// real sockets, socket options often have to be set before the connect() call,
// and the BeforeConnectCallback is the only way to do that, with a
// TCPClientSocket.
TEST_P(TCPSocketTest, BeforeConnectCallback) {
  // A receive buffer size that is between max and minimum buffer size limits,
  // and weird enough to likely not be a default value.
  const int kReceiveBufferSize = 32 * 1024 + 1117;
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  EXPECT_THAT(socket_->Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());

  connecting_socket.SetBeforeConnectCallback(base::BindLambdaForTesting([&] {
    EXPECT_FALSE(connecting_socket.IsConnected());
    int result = connecting_socket.SetReceiveBufferSize(kReceiveBufferSize);
    EXPECT_THAT(result, IsOk());
    return result;
  }));
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  int actual_size = 0;
  socklen_t actual_size_len = sizeof(actual_size);
  int os_result = getsockopt(
      connecting_socket.SocketDescriptorForTesting(), SOL_SOCKET, SO_RCVBUF,
      reinterpret_cast<char*>(&actual_size), &actual_size_len);
  ASSERT_EQ(0, os_result);
// Linux platforms generally allocate twice as much buffer size is requested to
// account for internal kernel data structures.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(2 * kReceiveBufferSize, actual_size);
// Unfortunately, Apple platform behavior doesn't seem to be documented, and
// doesn't match behavior on any other platforms.
// Fuchsia doesn't currently implement SO_RCVBUF.
#elif !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_FUCHSIA)
  EXPECT_EQ(kReceiveBufferSize, actual_size);
#endif
}

TEST_P(TCPSocketTest, BeforeConnectCallbackFails) {
  // Setting up a server isn't strictly necessary, but it does allow checking
  // the server was never connected to.
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  EXPECT_THAT(socket_->Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());

  // Set a callback that returns a nonsensical error, and make sure it's
  // returned.
  connecting_socket.SetBeforeConnectCallback(base::BindRepeating(
      [] { return static_cast<int>(net::ERR_NAME_NOT_RESOLVED); }));
  int connect_result = connecting_socket.Connect(connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result),
              IsError(net::ERR_NAME_NOT_RESOLVED));

  // Best effort check that the socket wasn't accepted - may flakily pass on
  // regression, unfortunately.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(accept_callback.have_result());
}

TEST_P(TCPSocketTest, SetKeepAlive) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  EXPECT_THAT(socket_->Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());

  // Non-connected sockets should not be able to set KeepAlive.
  ASSERT_FALSE(connecting_socket.IsConnected());
  EXPECT_FALSE(
      connecting_socket.SetKeepAlive(true /* enable */, 14 /* delay */));

  // Connect.
  int connect_result = connecting_socket.Connect(connect_callback.callback());
  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  // Connected sockets should be able to enable and disable KeepAlive.
  ASSERT_TRUE(connecting_socket.IsConnected());
  EXPECT_TRUE(
      connecting_socket.SetKeepAlive(true /* enable */, 22 /* delay */));
  EXPECT_TRUE(
      connecting_socket.SetKeepAlive(false /* enable */, 3 /* delay */));
}

TEST_P(TCPSocketTest, SetNoDelay) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  EXPECT_THAT(socket_->Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    nullptr, NetLogSource());

  // Non-connected sockets should not be able to set NoDelay.
  ASSERT_FALSE(connecting_socket.IsConnected());
  EXPECT_FALSE(connecting_socket.SetNoDelay(true /* no_delay */));

  // Connect.
  int connect_result = connecting_socket.Connect(connect_callback.callback());
  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  // Connected sockets should be able to enable and disable NoDelay.
  ASSERT_TRUE(connecting_socket.IsConnected());
  EXPECT_TRUE(connecting_socket.SetNoDelay(true /* no_delay */));
  EXPECT_TRUE(connecting_socket.SetNoDelay(false /* no_delay */));
}

// These tests require kernel support for tcp_info struct, and so they are
// enabled only on certain platforms.
#if defined(TCP_INFO) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// If SocketPerformanceWatcher::ShouldNotifyUpdatedRTT always returns false,
// then the wtatcher should not receive any notifications.
TEST_P(TCPSocketTest, SPWNotInterested) {
  TestSPWNotifications(false, 2u, 0u, 0u);
}

// One notification should be received when the socket connects. One
// additional notification should be received for each message read.
TEST_P(TCPSocketTest, SPWNoAdvance) {
  TestSPWNotifications(true, 2u, 0u, 3u);
}
#endif  // defined(TCP_INFO) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// On Android, where socket tagging is supported, verify that TCPSocket::Tag
// works as expected.
#if BUILDFLAG(IS_ANDROID)
TEST_P(TCPSocketTest, Tag) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  // Start test server.
  EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr_list;
  ASSERT_TRUE(test_server.GetAddressList(&addr_list));
  EXPECT_EQ(socket_->Open(addr_list[0].GetFamily()), OK);

  // Verify TCP connect packets are tagged and counted properly.
  int32_t tag_val1 = 0x12345678;
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  socket_->ApplySocketTag(tag1);
  TestCompletionCallback connect_callback;
  int connect_result =
      socket_->Connect(addr_list[0], connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  int32_t tag_val2 = 0x87654321;
  old_traffic = GetTaggedBytes(tag_val2);
  SocketTag tag2(getuid(), tag_val2);
  socket_->ApplySocketTag(tag2);
  const char kRequest1[] = "GET / HTTP/1.0";
  scoped_refptr<IOBuffer> write_buffer1 =
      base::MakeRefCounted<StringIOBuffer>(kRequest1);
  TestCompletionCallback write_callback1;
  EXPECT_EQ(
      socket_->Write(write_buffer1.get(), strlen(kRequest1),
                     write_callback1.callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      static_cast<int>(strlen(kRequest1)));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  old_traffic = GetTaggedBytes(tag_val1);
  socket_->ApplySocketTag(tag1);
  const char kRequest2[] = "\n\n";
  scoped_refptr<IOBuffer> write_buffer2 =
      base::MakeRefCounted<StringIOBuffer>(kRequest2);
  TestCompletionCallback write_callback2;
  EXPECT_EQ(
      socket_->Write(write_buffer2.get(), strlen(kRequest2),
                     write_callback2.callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      static_cast<int>(strlen(kRequest2)));
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  socket_->Close();
}

TEST_P(TCPSocketTest, TagAfterConnect) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  // Start test server.
  EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr_list;
  ASSERT_TRUE(test_server.GetAddressList(&addr_list));
  EXPECT_EQ(socket_->Open(addr_list[0].GetFamily()), OK);

  // Connect socket.
  TestCompletionCallback connect_callback;
  int connect_result =
      socket_->Connect(addr_list[0], connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  // Verify socket can be tagged with a new value and the current process's
  // UID.
  int32_t tag_val2 = 0x87654321;
  uint64_t old_traffic = GetTaggedBytes(tag_val2);
  SocketTag tag2(getuid(), tag_val2);
  socket_->ApplySocketTag(tag2);
  const char kRequest1[] = "GET / HTTP/1.0";
  scoped_refptr<IOBuffer> write_buffer1 =
      base::MakeRefCounted<StringIOBuffer>(kRequest1);
  TestCompletionCallback write_callback1;
  EXPECT_EQ(
      socket_->Write(write_buffer1.get(), strlen(kRequest1),
                     write_callback1.callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      static_cast<int>(strlen(kRequest1)));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  int32_t tag_val1 = 0x12345678;
  old_traffic = GetTaggedBytes(tag_val1);
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  socket_->ApplySocketTag(tag1);
  const char kRequest2[] = "\n\n";
  scoped_refptr<IOBuffer> write_buffer2 =
      base::MakeRefCounted<StringIOBuffer>(kRequest2);
  TestCompletionCallback write_callback2;
  EXPECT_EQ(
      socket_->Write(write_buffer2.get(), strlen(kRequest2),
                     write_callback2.callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      static_cast<int>(strlen(kRequest2)));
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  socket_->Close();
}

TEST_P(TCPSocketTest, BindToNetwork) {
  NetworkChangeNotifierFactoryAndroid ncn_factory;
  NetworkChangeNotifier::DisableForTest ncn_disable_for_test;
  std::unique_ptr<NetworkChangeNotifier> ncn(ncn_factory.CreateInstance());
  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    GTEST_SKIP() << "Network handles are required to test BindToNetwork.";

  const handles::NetworkHandle wrong_network_handle = 65536;
  // Try binding to this IP to trigger the underlying BindToNetwork call.
  const IPEndPoint ip(IPAddress::IPv4Localhost(), 0);
  // TestCompletionCallback connect_callback;
  TCPClientSocket wrong_socket(local_address_list(), nullptr, nullptr, nullptr,
                               NetLogSource(), wrong_network_handle);
  // Different Android versions might report different errors. Hence, just check
  // what shouldn't happen.
  int rv = wrong_socket.Bind(ip);
  EXPECT_NE(OK, rv);
  EXPECT_NE(ERR_NOT_IMPLEMENTED, rv);

  // Connecting using an existing network should succeed.
  const handles::NetworkHandle network_handle =
      NetworkChangeNotifier::GetDefaultNetwork();
  if (network_handle != handles::kInvalidNetworkHandle) {
    TCPClientSocket correct_socket(local_address_list(), nullptr, nullptr,
                                   nullptr, NetLogSource(), network_handle);
    EXPECT_EQ(OK, correct_socket.Bind(ip));
  }
}

#endif  // BUILDFLAG(IS_ANDROID)

// Tests error handling in write.
TEST_P(TCPSocketTest, WriteError) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [socket1, socket2] = CreateIPv4SocketPair();

  // Disallow send operations to make the next `Write` fail.
#if BUILDFLAG(IS_WIN)
  shutdown(socket1->SocketDescriptorForTesting(), SD_SEND);
#else
  shutdown(socket1->SocketDescriptorForTesting(), SHUT_WR);
#endif

  // Attempt to write data. It should fail.
  TestCompletionCallback write_callback;
  auto buffer = base::MakeRefCounted<StringIOBuffer>("test");
  int write_result =
      socket1->Write(buffer.get(), buffer->size(), write_callback.callback(),
                     TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_NE(write_result, net::OK);
  write_callback.GetResult(write_result);
}

// Tests error handling in read.
TEST_P(TCPSocketTest, ReadError) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [socket1, socket2] = CreateIPv4SocketPair();

  // Disallow receive operations to make the next `Read` fail.
#if BUILDFLAG(IS_WIN)
  shutdown(socket1->SocketDescriptorForTesting(), SD_RECEIVE);
#else
  shutdown(socket1->SocketDescriptorForTesting(), SHUT_RD);
#endif

  // Attempt to read data. It should fail.
  TestCompletionCallback read_callback;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(10);
  int read_result =
      socket1->Read(buffer.get(), buffer->size(), read_callback.callback());
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(read_result, net::ERR_FAILED);
#else
  // Ideally, this test should make the read return a failure code.
  // Unfortunately, we haven't found a good way to do that.
  EXPECT_EQ(read_result, net::OK);
#endif
  read_callback.GetResult(read_result);
}

// Tests error in a read that returns `net::ERR_IO_PENDING`.
TEST_P(TCPSocketTest, PendingReadError) {
#if BUILDFLAG(IS_WIN)
  if (!IsTcpSocketIoCompletionPortWinEnabled()) {
    // With the default implementation, the read callback is not invoked after
    // `CloseSocketDescriptorForTesting()` is invoked.
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  auto [socket1, socket2] = CreateIPv4SocketPair();

  // Attempt to read data. It should return `net::ERR_IO_PENDING` because no
  // data was written at the other end yet.
  TestCompletionCallback read_callback;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(10);
  int read_result =
      socket1->Read(buffer.get(), buffer->size(), read_callback.callback());
  EXPECT_EQ(read_result, net::ERR_IO_PENDING);

#if BUILDFLAG(IS_WIN)
  // Close the underlying socket to make the pending read fail.
  socket1->CloseSocketDescriptorForTesting();
#else
  // Disallow receive operations to make the pending read fail.
  shutdown(socket1->SocketDescriptorForTesting(), SHUT_RD);
#endif

  // The read operation should fail.
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(read_callback.GetResult(read_result), net::ERR_FAILED);
#else
  // Ideally, this test should make the read return a failure code.
  // Unfortunately, we haven't found a good way to do that.
  EXPECT_EQ(read_callback.GetResult(read_result), net::OK);
#endif
}

INSTANTIATE_TEST_SUITE_P(
    Any,
    TCPSocketTest,
    ::testing::Values(
        // Base tests
        std::make_tuple(false, false),  // Base, Read
        std::make_tuple(false, true)    // Base, ReadIfReady
#if BUILDFLAG(IS_WIN)
        // TcpSocketIoCompletionPortWin tests
        ,
        std::make_tuple(true,
                        false),      // TcpSocketIoCompletionPortWin, Read
        std::make_tuple(true, true)  // TcpSocketIoCompletionPortWin,
                                     // ReadIfReady
#endif
        ),
    [](::testing::TestParamInfo<std::tuple<bool, bool>> info) {
      std::string name =
          std::get<0>(info.param) ? "TcpSocketIoCompletionPortWin" : "Base";
      name += std::get<1>(info.param) ? "_ReadIfReady" : "_Read";
      return name;
    });

}  // namespace
}  // namespace net
