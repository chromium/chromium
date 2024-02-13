// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_server_socket_posix.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {
namespace {

const char kSocketFilename[] = "socket_for_testing";
const char kInvalidSocketPath[] = "/invalid/path";

bool UserCanConnectCallback(bool allow_user,
    const UnixDomainServerSocket::Credentials& credentials) {
  // Here peers are running in same process.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(getpid(), credentials.process_id);
#endif
  EXPECT_EQ(getuid(), credentials.user_id);
  EXPECT_EQ(getgid(), credentials.group_id);
  return allow_user;
}

UnixDomainServerSocket::AuthCallback CreateAuthCallback(bool allow_user) {
  return base::BindRepeating(&UserCanConnectCallback, allow_user);
}

class UnixDomainServerSocketTest : public testing::Test {
 protected:
  UnixDomainServerSocketTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.GetPath().Append(kSocketFilename).value();
  }

  base::ScopedTempDir temp_dir_;
  std::string socket_path_;
};

TEST_F(UnixDomainServerSocketTest, ListenWithInvalidPath) {
  const bool kUseAbstractNamespace = false;
  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            server_socket.BindAndListen(kInvalidSocketPath, /*backlog=*/1));
}

TEST_F(UnixDomainServerSocketTest, ListenWithInvalidPathWithAbstractNamespace) {
  const bool kUseAbstractNamespace = true;
  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_THAT(server_socket.BindAndListen(kInvalidSocketPath, /*backlog=*/1),
              IsOk());
#else
  EXPECT_EQ(ERR_ADDRESS_INVALID,
            server_socket.BindAndListen(kInvalidSocketPath, /*backlog=*/1));
#endif
}

TEST_F(UnixDomainServerSocketTest, ListenAgainAfterFailureWithInvalidPath) {
  const bool kUseAbstractNamespace = false;
  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);
  EXPECT_EQ(ERR_FILE_NOT_FOUND,
            server_socket.BindAndListen(kInvalidSocketPath, /*backlog=*/1));
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());
}

TEST_F(UnixDomainServerSocketTest, AcceptWithForbiddenUser) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  const bool kUseAbstractNamespace = false;

  UnixDomainServerSocket server_socket(CreateAuthCallback(false),
                                       kUseAbstractNamespace);
  EXPECT_THAT(server_socket.BindAndListen(socket_path_, /*backlog=*/1), IsOk());

  std::unique_ptr<StreamSocket> accepted_socket;
  TestCompletionCallback accept_callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server_socket.Accept(&accepted_socket, accept_callback.callback()));
  EXPECT_FALSE(accepted_socket);

  UnixDomainClientSocket client_socket(socket_path_, kUseAbstractNamespace);
  EXPECT_FALSE(client_socket.IsConnected());

  // Connect() will return OK before the server rejects the connection.
  TestCompletionCallback connect_callback;
  int rv = connect_callback.GetResult(
      client_socket.Connect(connect_callback.callback()));
  ASSERT_THAT(rv, IsOk());

  // Try to read from the socket.
  const int read_buffer_size = 10;
  auto read_buffer = base::MakeRefCounted<IOBufferWithSize>(read_buffer_size);
  TestCompletionCallback read_callback;
  rv = read_callback.GetResult(client_socket.Read(
      read_buffer.get(), read_buffer_size, read_callback.callback()));

  // The server should have disconnected gracefully, without sending any data.
  ASSERT_EQ(0, rv);
  EXPECT_FALSE(client_socket.IsConnected());

  // The server socket should not have called |accept_callback| or modified
  // |accepted_socket|.
  EXPECT_FALSE(accept_callback.have_result());
  EXPECT_FALSE(accepted_socket);
}

TEST_F(UnixDomainServerSocketTest, UnimplementedMethodsFail) {
  const bool kUseAbstractNamespace = false;
  UnixDomainServerSocket server_socket(CreateAuthCallback(true),
                                       kUseAbstractNamespace);

  IPEndPoint ep;
  EXPECT_THAT(server_socket.Listen(ep, 0, /*ipv6_only=*/std::nullopt),
              IsError(ERR_NOT_IMPLEMENTED));
  EXPECT_EQ(ERR_NOT_IMPLEMENTED,
      server_socket.ListenWithAddressAndPort(kInvalidSocketPath,
                                             0,
                                             /*backlog=*/1));

  EXPECT_THAT(server_socket.GetLocalAddress(&ep), IsError(ERR_ADDRESS_INVALID));
}

// Normal cases including read/write are tested by UnixDomainClientSocketTest.

}  // namespace
}  // namespace net
