// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_client_socket_posix.h"

#include <unistd.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/posix/eintr_wrapper.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_posix.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {
namespace {

const char kSocketFilename[] = "socket_for_testing";

bool UserCanConnectCallback(
    bool allow_user, const UnixDomainServerSocket::Credentials& credentials) {
  // Here peers are running in same process.
#if defined(OS_LINUX) || defined(OS_ANDROID)
  EXPECT_EQ(getpid(), credentials.process_id);
#endif
  EXPECT_EQ(getuid(), credentials.user_id);
  EXPECT_EQ(getgid(), credentials.group_id);
  return allow_user;
}

UnixDomainServerSocket::AuthCallback CreateAuthCallback(bool allow_user) {
  return base::Bind(&UserCanConnectCallback, allow_user);
}

// Connects socket synchronously.
int ConnectSynchronously(StreamSocket* socket) {
  TestCompletionCallback connect_callback;
  int rv = socket->Connect(connect_callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = connect_callback.WaitForResult();
  return rv;
}

// Reads data from |socket| until it fills |buf| at least up to |min_data_len|.
// Returns length of data read, or a net error.
int ReadSynchronously(StreamSocket* socket,
                      IOBuffer* buf,
                      int buf_len,
                      int min_data_len) {
  DCHECK_LE(min_data_len, buf_len);
  scoped_refptr<DrainableIOBuffer> read_buf =
      base::MakeRefCounted<DrainableIOBuffer>(buf, buf_len);
  TestCompletionCallback read_callback;
  // Iterate reading several times (but not infinite) until it reads at least
  // |min_data_len| bytes into |buf|.
  for (int retry_count = 10;
       retry_count > 0 && (read_buf->BytesConsumed() < min_data_len ||
                           // Try at least once when min_data_len == 0.
                           min_data_len == 0);
       --retry_count) {
    int rv = socket->Read(
        read_buf.get(), read_buf->BytesRemaining(), read_callback.callback());
    EXPECT_GE(read_buf->BytesRemaining(), rv);
    if (rv == ERR_IO_PENDING) {
      // If |min_data_len| is 0, returns ERR_IO_PENDING to distinguish the case
      // when some data has been read.
      if (min_data_len == 0) {
        // No data has been read because of for-loop condition.
        DCHECK_EQ(0, read_buf->BytesConsumed());
        return ERR_IO_PENDING;
      }
      rv = read_callback.WaitForResult();
    }
    EXPECT_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;
    read_buf->DidConsume(rv);
  }
  EXPECT_LE(0, read_buf->BytesRemaining());
  return read_buf->BytesConsumed();
}

// Writes data to |socket| until it completes writing |buf| up to |buf_len|.
// Returns length of data written, or a net error.
int WriteSynchronously(StreamSocket* socket,
                       IOBuffer* buf,
                       int buf_len) {
  scoped_refptr<DrainableIOBuffer> write_buf =
      base::MakeRefCounted<DrainableIOBuffer>(buf, buf_len);
  TestCompletionCallback write_callback;
  // Iterate writing several times (but not infinite) until it writes buf fully.
  for (int retry_count = 10;
       retry_count > 0 && write_buf->BytesRemaining() > 0;
       --retry_count) {
    int rv =
        socket->Write(write_buf.get(), write_buf->BytesRemaining(),
                      write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_GE(write_buf->BytesRemaining(), rv);
    if (rv == ERR_IO_PENDING)
      rv = write_callback.WaitForResult();
    EXPECT_NE(ERR_IO_PENDING, rv);
    if (rv < 0)
      return rv;
    write_buf->DidConsume(rv);
  }
  EXPECT_LE(0, write_buf->BytesRemaining());
  return write_buf->BytesConsumed();
}

class UnixDomainClientSocketTest : public TestWithTaskEnvironment {
 protected:
  UnixDomainClientSocketTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.GetPath().Append(kSocketFilename).value();
  }

  base::ScopedTempDir temp_dir_;
  std::string socket_path_;
};

TEST_F(UnixDomainClientSocketTest, Connect) {
  const bool kUseAbstractNamespace = false;

  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());

  std::unique_ptr<StreamSocket> accepted_socket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.Accept(&accepted_socket, accept_callback.callback()));
  EXPECT_FALSE(accepted_socket);

  UnixDomainClientSocket client_socket(socket_path_, kUseAbstractNamespace);
  EXPECT_FALSE(client_socket.IsConnected());

  EXPECT_THAT(ConnectSynchronously(&client_socket), IsOk());
  EXPECT_TRUE(client_socket.IsConnected());
  // Server has not yet been notified of the connection.
  EXPECT_FALSE(accepted_socket);

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_TRUE(accepted_socket);
  EXPECT_TRUE(accepted_socket->IsConnected());
}

TEST_F(UnixDomainClientSocketTest, ConnectWithSocketDescriptor) {
  const bool kUseAbstractNamespace = false;

  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());

  SocketDescriptor accepted_socket_fd = kInvalidSocket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.AcceptSocketDescriptor(&accepted_socket_fd,
                                                 accept_callback.callback()));
  EXPECT_EQ(kInvalidSocket, accepted_socket_fd);

  UnixDomainClientSocket client_socket(socket_path_, kUseAbstractNamespace);
  EXPECT_FALSE(client_socket.IsConnected());

  EXPECT_THAT(ConnectSynchronously(&client_socket), IsOk());
  EXPECT_TRUE(client_socket.IsConnected());
  // Server has not yet been notified of the connection.
  EXPECT_EQ(kInvalidSocket, accepted_socket_fd);

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_NE(kInvalidSocket, accepted_socket_fd);

  SocketDescriptor client_socket_fd = client_socket.ReleaseConnectedSocket();
  EXPECT_NE(kInvalidSocket, client_socket_fd);

  // Now, re-wrap client_socket_fd in a UnixDomainClientSocket and try a read
  // to be sure it hasn't gotten accidentally closed.
  SockaddrStorage addr;
  ASSERT_TRUE(UnixDomainClientSocket::FillAddress(socket_path_, false, &addr));
  std::unique_ptr<SocketPosix> adopter(new SocketPosix);
  adopter->AdoptConnectedSocket(client_socket_fd, addr);
  UnixDomainClientSocket rewrapped_socket(std::move(adopter));
  EXPECT_TRUE(rewrapped_socket.IsConnected());

  // Try to read data.
  const int kReadDataSize = 10;
  scoped_refptr<IOBuffer> read_buffer =
      base::MakeRefCounted<IOBuffer>(kReadDataSize);
  TestCompletionCallback read_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            rewrapped_socket.Read(
                read_buffer.get(), kReadDataSize, read_callback.callback()));

  EXPECT_EQ(0, IGNORE_EINTR(close(accepted_socket_fd)));
}

TEST_F(UnixDomainClientSocketTest, ConnectWithAbstractNamespace) {
  const bool kUseAbstractNamespace = true;

  UnixDomainClientSocket client_socket(socket_path_, kUseAbstractNamespace);
  EXPECT_FALSE(client_socket.IsConnected());

#if defined(OS_ANDROID) || defined(OS_LINUX)
  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());

  std::unique_ptr<StreamSocket> accepted_socket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.Accept(&accepted_socket, accept_callback.callback()));
  EXPECT_FALSE(accepted_socket);

  EXPECT_THAT(ConnectSynchronously(&client_socket), IsOk());
  EXPECT_TRUE(client_socket.IsConnected());
  // Server has not yet beend notified of the connection.
  EXPECT_FALSE(accepted_socket);

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_TRUE(accepted_socket);
  EXPECT_TRUE(accepted_socket->IsConnected());
#else
  EXPECT_THAT(ConnectSynchronously(&client_socket),
              IsError(ERR_ADDRESS_INVALID));
#endif
}

TEST_F(UnixDomainClientSocketTest, ConnectToNonExistentSocket) {
  const bool kUseAbstractNamespace = false;

  UnixDomainClientSocket client_socket(socket_path_, kUseAbstractNamespace);
  EXPECT_FALSE(client_socket.IsConnected());
  EXPECT_THAT(ConnectSynchronously(&client_socket),
              IsError(ERR_FILE_NOT_FOUND));
}

TEST_F(UnixDomainClientSocketTest,
       ConnectToNonExistentSocketWithAbstractNamespace) {
  const bool kUseAbstractNamespace = true;

  UnixDomainClientSocket client_socket(socket_path_, kUseAbstractNamespace);
  EXPECT_FALSE(client_socket.IsConnected());

  TestCompletionCallback connect_callback;
#if defined(OS_ANDROID) || defined(OS_LINUX)
  EXPECT_THAT(ConnectSynchronously(&client_socket),
              IsError(ERR_CONNECTION_REFUSED));
#else
  EXPECT_THAT(ConnectSynchronously(&client_socket),
              IsError(ERR_ADDRESS_INVALID));
#endif
}

TEST_F(UnixDomainClientSocketTest, DisconnectFromClient) {
  UnixDomainServerSocket server_socket(CreateAuthCallback(true), false);
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());
  std::unique_ptr<StreamSocket> accepted_socket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.Accept(&accepted_socket, accept_callback.callback()));
  UnixDomainClientSocket client_socket(socket_path_, false);
  EXPECT_THAT(ConnectSynchronously(&client_socket), IsOk());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_TRUE(accepted_socket->IsConnected());
  EXPECT_TRUE(client_socket.IsConnected());

  // Try to read data.
  const int kReadDataSize = 10;
  scoped_refptr<IOBuffer> read_buffer =
      base::MakeRefCounted<IOBuffer>(kReadDataSize);
  TestCompletionCallback read_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            accepted_socket->Read(
                read_buffer.get(), kReadDataSize, read_callback.callback()));

  // Disconnect from client side.
  client_socket.Disconnect();
  EXPECT_FALSE(client_socket.IsConnected());
  EXPECT_FALSE(accepted_socket->IsConnected());

  // Connection closed by peer.
  EXPECT_EQ(0 /* EOF */, read_callback.WaitForResult());
  // Note that read callback won't be called when the connection is closed
  // locally before the peer closes it. SocketPosix just clears callbacks.
}

TEST_F(UnixDomainClientSocketTest, DisconnectFromServer) {
  UnixDomainServerSocket server_socket(CreateAuthCallback(true), false);
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());
  std::unique_ptr<StreamSocket> accepted_socket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.Accept(&accepted_socket, accept_callback.callback()));
  UnixDomainClientSocket client_socket(socket_path_, false);
  EXPECT_THAT(ConnectSynchronously(&client_socket), IsOk());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_TRUE(accepted_socket->IsConnected());
  EXPECT_TRUE(client_socket.IsConnected());

  // Try to read data.
  const int kReadDataSize = 10;
  scoped_refptr<IOBuffer> read_buffer =
      base::MakeRefCounted<IOBuffer>(kReadDataSize);
  TestCompletionCallback read_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            client_socket.Read(
                read_buffer.get(), kReadDataSize, read_callback.callback()));

  // Disconnect from server side.
  accepted_socket->Disconnect();
  EXPECT_FALSE(accepted_socket->IsConnected());
  EXPECT_FALSE(client_socket.IsConnected());

  // Connection closed by peer.
  EXPECT_EQ(0 /* EOF */, read_callback.WaitForResult());
  // Note that read callback won't be called when the connection is closed
  // locally before the peer closes it. SocketPosix just clears callbacks.
}

TEST_F(UnixDomainClientSocketTest, ReadAfterWrite) {
  UnixDomainServerSocket server_socket(CreateAuthCallback(true), false);
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());
  std::unique_ptr<StreamSocket> accepted_socket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.Accept(&accepted_socket, accept_callback.callback()));
  UnixDomainClientSocket client_socket(socket_path_, false);
  EXPECT_THAT(ConnectSynchronously(&client_socket), IsOk());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_TRUE(accepted_socket->IsConnected());
  EXPECT_TRUE(client_socket.IsConnected());

  // Send data from client to server.
  const int kWriteDataSize = 10;
  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(std::string(kWriteDataSize, 'd'));
  EXPECT_EQ(
      kWriteDataSize,
      WriteSynchronously(&client_socket, write_buffer.get(), kWriteDataSize));

  // The buffer is bigger than write data size.
  const int kReadBufferSize = kWriteDataSize * 2;
  scoped_refptr<IOBuffer> read_buffer =
      base::MakeRefCounted<IOBuffer>(kReadBufferSize);
  EXPECT_EQ(kWriteDataSize,
            ReadSynchronously(accepted_socket.get(),
                              read_buffer.get(),
                              kReadBufferSize,
                              kWriteDataSize));
  EXPECT_EQ(std::string(write_buffer->data(), kWriteDataSize),
            std::string(read_buffer->data(), kWriteDataSize));

  // Send data from server and client.
  EXPECT_EQ(kWriteDataSize,
            WriteSynchronously(
                accepted_socket.get(), write_buffer.get(), kWriteDataSize));

  // Read multiple times.
  const int kSmallReadBufferSize = kWriteDataSize / 3;
  EXPECT_EQ(kSmallReadBufferSize,
            ReadSynchronously(&client_socket,
                              read_buffer.get(),
                              kSmallReadBufferSize,
                              kSmallReadBufferSize));
  EXPECT_EQ(std::string(write_buffer->data(), kSmallReadBufferSize),
            std::string(read_buffer->data(), kSmallReadBufferSize));

  EXPECT_EQ(kWriteDataSize - kSmallReadBufferSize,
            ReadSynchronously(&client_socket,
                              read_buffer.get(),
                              kReadBufferSize,
                              kWriteDataSize - kSmallReadBufferSize));
  EXPECT_EQ(std::string(write_buffer->data() + kSmallReadBufferSize,
                        kWriteDataSize - kSmallReadBufferSize),
            std::string(read_buffer->data(),
                        kWriteDataSize - kSmallReadBufferSize));

  // No more data.
  EXPECT_EQ(
      ERR_IO_PENDING,
      ReadSynchronously(&client_socket, read_buffer.get(), kReadBufferSize, 0));

  // Disconnect from server side after read-write.
  accepted_socket->Disconnect();
  EXPECT_FALSE(accepted_socket->IsConnected());
  EXPECT_FALSE(client_socket.IsConnected());
}

TEST_F(UnixDomainClientSocketTest, ReadBeforeWrite) {
  UnixDomainServerSocket server_socket(CreateAuthCallback(true), false);
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());
  std::unique_ptr<StreamSocket> accepted_socket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.Accept(&accepted_socket, accept_callback.callback()));
  UnixDomainClientSocket client_socket(socket_path_, false);
  EXPECT_THAT(ConnectSynchronously(&client_socket), IsOk());

  EXPECT_THAT(accept_callback.WaitForResult(), IsOk());
  EXPECT_TRUE(accepted_socket->IsConnected());
  EXPECT_TRUE(client_socket.IsConnected());

  // Wait for data from client.
  const int kWriteDataSize = 10;
  const int kReadBufferSize = kWriteDataSize * 2;
  const int kSmallReadBufferSize = kWriteDataSize / 3;
  // Read smaller than write data size first.
  scoped_refptr<IOBuffer> read_buffer =
      base::MakeRefCounted<IOBuffer>(kReadBufferSize);
  TestCompletionCallback read_callback;
  EXPECT_EQ(
      ERR_IO_PENDING,
      accepted_socket->Read(
          read_buffer.get(), kSmallReadBufferSize, read_callback.callback()));

  scoped_refptr<IOBuffer> write_buffer =
      base::MakeRefCounted<StringIOBuffer>(std::string(kWriteDataSize, 'd'));
  EXPECT_EQ(
      kWriteDataSize,
      WriteSynchronously(&client_socket, write_buffer.get(), kWriteDataSize));

  // First read completed.
  int rv = read_callback.WaitForResult();
  EXPECT_LT(0, rv);
  EXPECT_LE(rv, kSmallReadBufferSize);

  // Read remaining data.
  const int kExpectedRemainingDataSize = kWriteDataSize - rv;
  EXPECT_LE(0, kExpectedRemainingDataSize);
  EXPECT_EQ(kExpectedRemainingDataSize,
            ReadSynchronously(accepted_socket.get(),
                              read_buffer.get(),
                              kReadBufferSize,
                              kExpectedRemainingDataSize));
  // No more data.
  EXPECT_EQ(ERR_IO_PENDING,
            ReadSynchronously(
                accepted_socket.get(), read_buffer.get(), kReadBufferSize, 0));

  // Disconnect from server side after read-write.
  accepted_socket->Disconnect();
  EXPECT_FALSE(accepted_socket->IsConnected());
  EXPECT_FALSE(client_socket.IsConnected());
}

}  // namespace
}  // namespace net
