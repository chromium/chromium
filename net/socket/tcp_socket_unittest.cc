// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket.h"

#include <stddef.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
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

// For getsockopt() call.
#if defined(OS_WIN)
#include <winsock2.h>
#else  // !defined(OS_WIN)
#include <sys/socket.h>
#endif  //  !defined(OS_WIN)

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
      : should_notify_updated_rtt_(should_notify_updated_rtt),
        connection_changed_count_(0u),
        rtt_notification_count_(0u) {}
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
  size_t connection_changed_count_;
  size_t rtt_notification_count_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketPerformanceWatcher);
};

const int kListenBacklog = 5;

class TCPSocketTest : public PlatformTest, public WithTaskEnvironment {
 protected:
  TCPSocketTest() : socket_(nullptr, nullptr, NetLogSource()) {}

  void SetUpListenIPv4() {
    ASSERT_THAT(socket_.Open(ADDRESS_FAMILY_IPV4), IsOk());
    ASSERT_THAT(socket_.Bind(IPEndPoint(IPAddress::IPv4Localhost(), 0)),
                IsOk());
    ASSERT_THAT(socket_.Listen(kListenBacklog), IsOk());
    ASSERT_THAT(socket_.GetLocalAddress(&local_address_), IsOk());
  }

  void SetUpListenIPv6(bool* success) {
    *success = false;

    if (socket_.Open(ADDRESS_FAMILY_IPV6) != OK ||
        socket_.Bind(IPEndPoint(IPAddress::IPv6Localhost(), 0)) != OK ||
        socket_.Listen(kListenBacklog) != OK) {
      LOG(ERROR) << "Failed to listen on ::1 - probably because IPv6 is "
          "disabled. Skipping the test";
      return;
    }
    ASSERT_THAT(socket_.GetLocalAddress(&local_address_), IsOk());
    *success = true;
  }

  void TestAcceptAsync() {
    TestCompletionCallback accept_callback;
    std::unique_ptr<TCPSocket> accepted_socket;
    IPEndPoint accepted_address;
    ASSERT_THAT(socket_.Accept(&accepted_socket, &accepted_address,
                               accept_callback.callback()),
                IsError(ERR_IO_PENDING));

    TestCompletionCallback connect_callback;
    TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                      NetLogSource());
    int connect_result = connecting_socket.Connect(connect_callback.callback());
    EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

    EXPECT_THAT(accept_callback.WaitForResult(), IsOk());

    EXPECT_TRUE(accepted_socket.get());

    // Both sockets should be on the loopback network interface.
    EXPECT_EQ(accepted_address.address(), local_address_.address());
  }

#if defined(TCP_INFO) || defined(OS_LINUX)
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

    std::unique_ptr<TestSocketPerformanceWatcher> watcher(
        new TestSocketPerformanceWatcher(should_notify_updated_rtt));
    TestSocketPerformanceWatcher* watcher_ptr = watcher.get();

    TCPSocket connecting_socket(std::move(watcher), nullptr, NetLogSource());

    int result = connecting_socket.Open(ADDRESS_FAMILY_IPV4);
    ASSERT_THAT(result, IsOk());
    int connect_result =
        connecting_socket.Connect(local_address_, connect_callback.callback());

    TestCompletionCallback accept_callback;
    std::unique_ptr<TCPSocket> accepted_socket;
    IPEndPoint accepted_address;
    result = socket_.Accept(&accepted_socket, &accepted_address,
                            accept_callback.callback());
    ASSERT_THAT(accept_callback.GetResult(result), IsOk());

    ASSERT_TRUE(accepted_socket.get());

    // Both sockets should be on the loopback network interface.
    EXPECT_EQ(accepted_address.address(), local_address_.address());

    ASSERT_THAT(connect_callback.GetResult(connect_result), IsOk());

    for (size_t i = 0; i < num_messages; ++i) {
      // Use a 1 byte message so that the watcher is notified at most once per
      // message.
      const std::string message("t");

      scoped_refptr<IOBufferWithSize> write_buffer =
          base::MakeRefCounted<IOBufferWithSize>(message.size());
      memmove(write_buffer->data(), message.data(), message.size());

      TestCompletionCallback write_callback;
      int write_result = accepted_socket->Write(
          write_buffer.get(), write_buffer->size(), write_callback.callback(),
          TRAFFIC_ANNOTATION_FOR_TESTS);

      scoped_refptr<IOBufferWithSize> read_buffer =
          base::MakeRefCounted<IOBufferWithSize>(message.size());
      TestCompletionCallback read_callback;
      int read_result = connecting_socket.Read(
          read_buffer.get(), read_buffer->size(), read_callback.callback());

      ASSERT_EQ(1, write_callback.GetResult(write_result));
      ASSERT_EQ(1, read_callback.GetResult(read_result));
    }
    EXPECT_EQ(expect_connection_changed_count,
              watcher_ptr->connection_changed_count());
    EXPECT_EQ(expect_rtt_notification_count,
              watcher_ptr->rtt_notification_count());
  }
#endif  // defined(TCP_INFO) || defined(OS_LINUX)

  AddressList local_address_list() const {
    return AddressList(local_address_);
  }

  TCPSocket socket_;
  IPEndPoint local_address_;
};

// Test listening and accepting with a socket bound to an IPv4 address.
TEST_F(TCPSocketTest, Accept) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback connect_callback;
  // TODO(yzshen): Switch to use TCPSocket when it supports client socket
  // operations.
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  int result = socket_.Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  EXPECT_TRUE(accepted_socket.get());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(accepted_address.address(), local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

// Test Accept() callback.
TEST_F(TCPSocketTest, AcceptAsync) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());
  TestAcceptAsync();
}

// Test AdoptConnectedSocket()
TEST_F(TCPSocketTest, AdoptConnectedSocket) {
  TCPSocket accepting_socket(nullptr, nullptr, NetLogSource());
  ASSERT_THAT(accepting_socket.Open(ADDRESS_FAMILY_IPV4), IsOk());
  ASSERT_THAT(accepting_socket.Bind(IPEndPoint(IPAddress::IPv4Localhost(), 0)),
              IsOk());
  ASSERT_THAT(accepting_socket.GetLocalAddress(&local_address_), IsOk());
  ASSERT_THAT(accepting_socket.Listen(kListenBacklog), IsOk());

  TestCompletionCallback connect_callback;
  // TODO(yzshen): Switch to use TCPSocket when it supports client socket
  // operations.
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  int result = accepting_socket.Accept(&accepted_socket, &accepted_address,
                                       accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  SocketDescriptor accepted_descriptor =
      accepted_socket->ReleaseSocketDescriptorForTesting();

  ASSERT_THAT(
      socket_.AdoptConnectedSocket(accepted_descriptor, accepted_address),
      IsOk());

  // socket_ should now have the local address.
  IPEndPoint adopted_address;
  ASSERT_THAT(socket_.GetLocalAddress(&adopted_address), IsOk());
  EXPECT_EQ(local_address_.address(), adopted_address.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

// Test Accept() for AdoptUnconnectedSocket.
TEST_F(TCPSocketTest, AcceptForAdoptedUnconnectedSocket) {
  SocketDescriptor existing_socket =
      CreatePlatformSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_THAT(socket_.AdoptUnconnectedSocket(existing_socket), IsOk());

  IPEndPoint address(IPAddress::IPv4Localhost(), 0);
  SockaddrStorage storage;
  ASSERT_TRUE(address.ToSockAddr(storage.addr, &storage.addr_len));
  ASSERT_EQ(0, bind(existing_socket, storage.addr, storage.addr_len));

  ASSERT_THAT(socket_.Listen(kListenBacklog), IsOk());
  ASSERT_THAT(socket_.GetLocalAddress(&local_address_), IsOk());

  TestAcceptAsync();
}

// Accept two connections simultaneously.
TEST_F(TCPSocketTest, Accept2Connections) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;

  ASSERT_THAT(socket_.Accept(&accepted_socket, &accepted_address,
                             accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback connect_callback2;
  TCPClientSocket connecting_socket2(local_address_list(), nullptr, nullptr,
                                     NetLogSource());
  int connect_result2 =
      connecting_socket2.Connect(connect_callback2.callback());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());

  TestCompletionCallback accept_callback2;
  std::unique_ptr<TCPSocket> accepted_socket2;
  IPEndPoint accepted_address2;

  int result = socket_.Accept(&accepted_socket2, &accepted_address2,
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
TEST_F(TCPSocketTest, AcceptIPv6) {
  bool initialized = false;
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv6(&initialized));
  if (!initialized)
    return;

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());
  int connect_result = connecting_socket.Connect(connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  int result = socket_.Accept(&accepted_socket, &accepted_address,
                              accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  EXPECT_TRUE(accepted_socket.get());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(accepted_address.address(), local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
}

TEST_F(TCPSocketTest, ReadWrite) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback connect_callback;
  TCPSocket connecting_socket(nullptr, nullptr, NetLogSource());
  int result = connecting_socket.Open(ADDRESS_FAMILY_IPV4);
  ASSERT_THAT(result, IsOk());
  int connect_result =
      connecting_socket.Connect(local_address_, connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  result = socket_.Accept(&accepted_socket, &accepted_address,
                          accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());

  ASSERT_TRUE(accepted_socket.get());

  // Both sockets should be on the loopback network interface.
  EXPECT_EQ(accepted_address.address(), local_address_.address());

  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  const std::string message("test message");
  std::vector<char> buffer(message.size());

  size_t bytes_written = 0;
  while (bytes_written < message.size()) {
    scoped_refptr<IOBufferWithSize> write_buffer =
        base::MakeRefCounted<IOBufferWithSize>(message.size() - bytes_written);
    memmove(write_buffer->data(), message.data() + bytes_written,
            message.size() - bytes_written);

    TestCompletionCallback write_callback;
    int write_result = accepted_socket->Write(
        write_buffer.get(), write_buffer->size(), write_callback.callback(),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    write_result = write_callback.GetResult(write_result);
    ASSERT_TRUE(write_result >= 0);
    bytes_written += write_result;
    ASSERT_TRUE(bytes_written <= message.size());
  }

  size_t bytes_read = 0;
  while (bytes_read < message.size()) {
    scoped_refptr<IOBufferWithSize> read_buffer =
        base::MakeRefCounted<IOBufferWithSize>(message.size() - bytes_read);
    TestCompletionCallback read_callback;
    int read_result = connecting_socket.Read(
        read_buffer.get(), read_buffer->size(), read_callback.callback());
    read_result = read_callback.GetResult(read_result);
    ASSERT_TRUE(read_result >= 0);
    ASSERT_TRUE(bytes_read + read_result <= message.size());
    memmove(&buffer[bytes_read], read_buffer->data(), read_result);
    bytes_read += read_result;
  }

  std::string received_message(buffer.begin(), buffer.end());
  ASSERT_EQ(message, received_message);
}

// Destroy a TCPSocket while there's a pending read, and make sure the read
// IOBuffer that the socket was holding on to is destroyed.
// See https://crbug.com/804868.
TEST_F(TCPSocketTest, DestroyWithPendingRead) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  // Create a connected socket.

  TestCompletionCallback connect_callback;
  std::unique_ptr<TCPSocket> connecting_socket =
      std::make_unique<TCPSocket>(nullptr, nullptr, NetLogSource());
  int result = connecting_socket->Open(ADDRESS_FAMILY_IPV4);
  ASSERT_THAT(result, IsOk());
  int connect_result =
      connecting_socket->Connect(local_address_, connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  result = socket_.Accept(&accepted_socket, &accepted_address,
                          accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());
  ASSERT_TRUE(accepted_socket.get());
  ASSERT_THAT(connect_callback.GetResult(connect_result), IsOk());

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
TEST_F(TCPSocketTest, DestroyWithPendingWrite) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  // Create a connected socket.

  TestCompletionCallback connect_callback;
  std::unique_ptr<TCPSocket> connecting_socket =
      std::make_unique<TCPSocket>(nullptr, nullptr, NetLogSource());
  int result = connecting_socket->Open(ADDRESS_FAMILY_IPV4);
  ASSERT_THAT(result, IsOk());
  int connect_result =
      connecting_socket->Connect(local_address_, connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  result = socket_.Accept(&accepted_socket, &accepted_address,
                          accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());
  ASSERT_TRUE(accepted_socket.get());
  ASSERT_THAT(connect_callback.GetResult(connect_result), IsOk());

  // Repeatedly write to the socket until an operation does not complete
  // synchronously.
  base::RunLoop run_loop;
  scoped_refptr<IOBufferWithDestructionCallback> write_buffer(
      base::MakeRefCounted<IOBufferWithDestructionCallback>(
          run_loop.QuitClosure()));
  memset(write_buffer->data(), '1', write_buffer->size());
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
TEST_F(TCPSocketTest, CancelPendingReadIfReady) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  // Create a connected socket.
  TestCompletionCallback connect_callback;
  std::unique_ptr<TCPSocket> connecting_socket =
      std::make_unique<TCPSocket>(nullptr, nullptr, NetLogSource());
  int result = connecting_socket->Open(ADDRESS_FAMILY_IPV4);
  ASSERT_THAT(result, IsOk());
  int connect_result =
      connecting_socket->Connect(local_address_, connect_callback.callback());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  result = socket_.Accept(&accepted_socket, &accepted_address,
                          accept_callback.callback());
  ASSERT_THAT(accept_callback.GetResult(result), IsOk());
  ASSERT_TRUE(accepted_socket.get());
  ASSERT_THAT(connect_callback.GetResult(connect_result), IsOk());

  // Try to read from the socket, but never write anything to the other end.
  base::RunLoop run_loop;
  scoped_refptr<IOBufferWithDestructionCallback> read_buffer(
      base::MakeRefCounted<IOBufferWithDestructionCallback>(
          run_loop.QuitClosure()));
  TestCompletionCallback read_callback;
  EXPECT_EQ(ERR_IO_PENDING, connecting_socket->ReadIfReady(
                                read_buffer.get(), read_buffer->size(),
                                read_callback.callback()));

  // Now cancel the pending ReadIfReady().
  connecting_socket->CancelReadIfReady();

  // Send data to |connecting_socket|.
  const char kMsg[] = "hello!";
  scoped_refptr<StringIOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(kMsg);

  TestCompletionCallback write_callback;
  int write_result = accepted_socket->Write(write_buffer.get(), strlen(kMsg),
                                            write_callback.callback(),
                                            TRAFFIC_ANNOTATION_FOR_TESTS);
  const int msg_size = strlen(kMsg);
  ASSERT_EQ(msg_size, write_result);

  TestCompletionCallback read_callback2;
  int read_result = connecting_socket->ReadIfReady(
      read_buffer.get(), read_buffer->size(), read_callback2.callback());
  if (read_result == ERR_IO_PENDING) {
    ASSERT_EQ(OK, read_callback2.GetResult(read_result));
    read_result = connecting_socket->ReadIfReady(
        read_buffer.get(), read_buffer->size(), read_callback2.callback());
  }

  ASSERT_EQ(msg_size, read_result);
  ASSERT_EQ(0, memcmp(&kMsg, read_buffer->data(), msg_size));
}

TEST_F(TCPSocketTest, IsConnected) {
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  EXPECT_THAT(socket_.Accept(&accepted_socket, &accepted_address,
                             accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());

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
  FD_ZERO(&read_fds);
  SocketDescriptor connecting_fd =
      connecting_socket.SocketDescriptorForTesting();
  FD_SET(connecting_fd, &read_fds);
  ASSERT_EQ(select(FD_SETSIZE, &read_fds, nullptr, nullptr, nullptr), 1);
  ASSERT_TRUE(FD_ISSET(connecting_fd, &read_fds));

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
TEST_F(TCPSocketTest, BeforeConnectCallback) {
  // A receive buffer size that is between max and minimum buffer size limits,
  // and weird enough to likely not be a default value.
  const int kReceiveBufferSize = 32 * 1024 + 1117;
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  EXPECT_THAT(socket_.Accept(&accepted_socket, &accepted_address,
                             accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());

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
#if defined(OS_LINUX) || defined(OS_ANDROID)
  EXPECT_EQ(2 * kReceiveBufferSize, actual_size);
// Unfortunately, Apple platform behavior doesn't seem to be documented, and
// doesn't match behavior on any other platforms.
// Fuchsia doesn't currently implement SO_RCVBUF.
#elif !defined(OS_IOS) && !defined(OS_MACOSX) && !defined(OS_FUCHSIA)
  EXPECT_EQ(kReceiveBufferSize, actual_size);
#endif
}

TEST_F(TCPSocketTest, BeforeConnectCallbackFails) {
  // Setting up a server isn't strictly necessary, but it does allow checking
  // the server was never connected to.
  ASSERT_NO_FATAL_FAILURE(SetUpListenIPv4());

  TestCompletionCallback accept_callback;
  std::unique_ptr<TCPSocket> accepted_socket;
  IPEndPoint accepted_address;
  EXPECT_THAT(socket_.Accept(&accepted_socket, &accepted_address,
                             accept_callback.callback()),
              IsError(ERR_IO_PENDING));

  TestCompletionCallback connect_callback;
  TCPClientSocket connecting_socket(local_address_list(), nullptr, nullptr,
                                    NetLogSource());

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

// These tests require kernel support for tcp_info struct, and so they are
// enabled only on certain platforms.
#if defined(TCP_INFO) || defined(OS_LINUX)
// If SocketPerformanceWatcher::ShouldNotifyUpdatedRTT always returns false,
// then the wtatcher should not receive any notifications.
TEST_F(TCPSocketTest, SPWNotInterested) {
  TestSPWNotifications(false, 2u, 0u, 0u);
}

// One notification should be received when the socket connects. One
// additional notification should be received for each message read.
TEST_F(TCPSocketTest, SPWNoAdvance) {
  TestSPWNotifications(true, 2u, 0u, 3u);
}
#endif  // defined(TCP_INFO) || defined(OS_LINUX)

// On Android, where socket tagging is supported, verify that TCPSocket::Tag
// works as expected.
#if defined(OS_ANDROID)
TEST_F(TCPSocketTest, Tag) {
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
  EXPECT_EQ(socket_.Open(addr_list[0].GetFamily()), OK);

  // Verify TCP connect packets are tagged and counted properly.
  int32_t tag_val1 = 0x12345678;
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  socket_.ApplySocketTag(tag1);
  TestCompletionCallback connect_callback;
  int connect_result =
      socket_.Connect(addr_list[0], connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  int32_t tag_val2 = 0x87654321;
  old_traffic = GetTaggedBytes(tag_val2);
  SocketTag tag2(getuid(), tag_val2);
  socket_.ApplySocketTag(tag2);
  const char kRequest1[] = "GET / HTTP/1.0";
  scoped_refptr<IOBuffer> write_buffer1 =
      base::MakeRefCounted<StringIOBuffer>(kRequest1);
  TestCompletionCallback write_callback1;
  EXPECT_EQ(
      socket_.Write(write_buffer1.get(), strlen(kRequest1),
                    write_callback1.callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      static_cast<int>(strlen(kRequest1)));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  old_traffic = GetTaggedBytes(tag_val1);
  socket_.ApplySocketTag(tag1);
  const char kRequest2[] = "\n\n";
  scoped_refptr<IOBuffer> write_buffer2 =
      base::MakeRefCounted<StringIOBuffer>(kRequest2);
  TestCompletionCallback write_callback2;
  EXPECT_EQ(
      socket_.Write(write_buffer2.get(), strlen(kRequest2),
                    write_callback2.callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      static_cast<int>(strlen(kRequest2)));
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  socket_.Close();
}

TEST_F(TCPSocketTest, TagAfterConnect) {
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
  EXPECT_EQ(socket_.Open(addr_list[0].GetFamily()), OK);

  // Connect socket.
  TestCompletionCallback connect_callback;
  int connect_result =
      socket_.Connect(addr_list[0], connect_callback.callback());
  EXPECT_THAT(connect_callback.GetResult(connect_result), IsOk());

  // Verify socket can be tagged with a new value and the current process's
  // UID.
  int32_t tag_val2 = 0x87654321;
  uint64_t old_traffic = GetTaggedBytes(tag_val2);
  SocketTag tag2(getuid(), tag_val2);
  socket_.ApplySocketTag(tag2);
  const char kRequest1[] = "GET / HTTP/1.0";
  scoped_refptr<IOBuffer> write_buffer1 =
      base::MakeRefCounted<StringIOBuffer>(kRequest1);
  TestCompletionCallback write_callback1;
  EXPECT_EQ(
      socket_.Write(write_buffer1.get(), strlen(kRequest1),
                    write_callback1.callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      static_cast<int>(strlen(kRequest1)));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  int32_t tag_val1 = 0x12345678;
  old_traffic = GetTaggedBytes(tag_val1);
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  socket_.ApplySocketTag(tag1);
  const char kRequest2[] = "\n\n";
  scoped_refptr<IOBuffer> write_buffer2 =
      base::MakeRefCounted<StringIOBuffer>(kRequest2);
  TestCompletionCallback write_callback2;
  EXPECT_EQ(
      socket_.Write(write_buffer2.get(), strlen(kRequest2),
                    write_callback2.callback(), TRAFFIC_ANNOTATION_FOR_TESTS),
      static_cast<int>(strlen(kRequest2)));
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  socket_.Close();
}
#endif

}  // namespace
}  // namespace net
