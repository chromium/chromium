// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <sys/socket.h>

#include <cstdint>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/sockaddr_util_posix.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_posix.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/host/security_key/security_key_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const char kSocketFilename[] = "socket_for_testing";

// Test security key request data.
const uint8_t kRequestData[] = {
    0x00, 0x00, 0x00, 0x9a, 0x65, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x60, 0x90,
    0x24, 0x71, 0xf8, 0xf2, 0xe5, 0xdf, 0x7f, 0x81, 0xc7, 0x49, 0xc4, 0xa3,
    0x58, 0x5c, 0xf6, 0xcc, 0x40, 0x14, 0x28, 0x0c, 0xa0, 0xfa, 0x03, 0x18,
    0x38, 0xd8, 0x7d, 0x77, 0x2b, 0x3a, 0x00, 0x00, 0x00, 0x20, 0x64, 0x46,
    0x47, 0x2f, 0xdf, 0x6e, 0xed, 0x7b, 0xf3, 0xc3, 0x37, 0x20, 0xf2, 0x36,
    0x67, 0x6c, 0x36, 0xe1, 0xb4, 0x5e, 0xbe, 0x04, 0x85, 0xdb, 0x89, 0xa3,
    0xcd, 0xfd, 0xd2, 0x4b, 0xd6, 0x9f, 0x00, 0x00, 0x00, 0x40, 0x38, 0x35,
    0x05, 0x75, 0x1d, 0x13, 0x6e, 0xb3, 0x6b, 0x1d, 0x29, 0xae, 0xd3, 0x43,
    0xe6, 0x84, 0x8f, 0xa3, 0x9d, 0x65, 0x4e, 0x2f, 0x57, 0xe3, 0xf6, 0xe6,
    0x20, 0x3c, 0x00, 0xc6, 0xe1, 0x73, 0x34, 0xe2, 0x23, 0x99, 0xc4, 0xfa,
    0x91, 0xc2, 0xd5, 0x97, 0xc1, 0x8b, 0xd0, 0x3c, 0x13, 0xba, 0xf0, 0xd7,
    0x5e, 0xa3, 0xbc, 0x02, 0x5b, 0xec, 0xe4, 0x4b, 0xae, 0x0e, 0xf2, 0xbd,
    0xc8, 0xaa};

const uint8_t kResponseData[] = {0x00, 0x00, 0x00, 0x01, 0x42};

const uint8_t kSshErrorData[] = {0x00, 0x00, 0x00, 0x01, 0x05};

void RunUntilIdle() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

}  // namespace

class SecurityKeyAuthHandlerPosixTest : public testing::Test {
 public:
  SecurityKeyAuthHandlerPosixTest()
      : run_loop_(new base::RunLoop()),
        file_thread_("SecurityKeyAuthHandlerPosixTest_FileThread"),
        expected_request_data_(reinterpret_cast<const char*>(kRequestData + 4),
                               sizeof(kRequestData) - 4),
        client_response_data_(reinterpret_cast<const char*>(kResponseData + 4),
                              sizeof(kResponseData) - 4) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    socket_path_ = temp_dir_.GetPath().Append(kSocketFilename);
    remoting::SecurityKeyAuthHandler::SetSecurityKeySocketName(socket_path_);

    EXPECT_TRUE(file_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));

    send_message_callback_ = base::BindRepeating(
        &SecurityKeyAuthHandlerPosixTest::SendMessageToClient,
        base::Unretained(this));

    auth_handler_ = remoting::SecurityKeyAuthHandler::Create(
        /*client_session_details=*/nullptr, send_message_callback_,
        file_thread_.task_runner());
    EXPECT_NE(auth_handler_.get(), nullptr);
  }

  SecurityKeyAuthHandlerPosixTest(const SecurityKeyAuthHandlerPosixTest&) =
      delete;
  SecurityKeyAuthHandlerPosixTest& operator=(
      const SecurityKeyAuthHandlerPosixTest&) = delete;

  void CreateSocketAndWait() {
    ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
    auth_handler_->CreateSecurityKeyConnection();

    ASSERT_TRUE(file_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&RunUntilIdle), run_loop_->QuitClosure()));
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();

    ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
  }

  void SendMessageToClient(int connection_id, const std::string& data) {
    last_connection_id_received_ = connection_id;
    last_message_received_ = data;
    run_loop_->Quit();
  }

  void WaitForSendMessageToClient() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void CheckHostDataMessage(int id) {
    ASSERT_EQ(id, last_connection_id_received_);
    ASSERT_EQ(expected_request_data_.length(), last_message_received_.length());
    ASSERT_EQ(expected_request_data_, last_message_received_);
  }

  void WriteRequestData(net::UnixDomainClientSocket* client_socket) {
    int request_len = sizeof(kRequestData);
    auto request_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
        base::MakeRefCounted<net::WrappedIOBuffer>(kRequestData), request_len);
    net::TestCompletionCallback write_callback;
    int bytes_written = 0;
    while (bytes_written < request_len) {
      int write_result = client_socket->Write(
          request_buffer.get(), request_buffer->BytesRemaining(),
          write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
      write_result = write_callback.GetResult(write_result);
      ASSERT_GT(write_result, 0);
      bytes_written += write_result;
      ASSERT_LE(bytes_written, request_len);
      request_buffer->DidConsume(write_result);
    }
    ASSERT_EQ(request_len, bytes_written);
  }

  void WaitForResponseData(net::UnixDomainClientSocket* client_socket) {
    WaitForData(client_socket, sizeof(kResponseData));
  }

  void WaitForErrorData(net::UnixDomainClientSocket* client_socket) {
    WaitForData(client_socket, sizeof(kSshErrorData));
  }

  void WaitForData(net::UnixDomainClientSocket* socket, int request_len) {
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(request_len);
    auto read_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
        std::move(buffer), request_len);
    net::TestCompletionCallback read_callback;
    int bytes_read = 0;
    while (bytes_read < request_len) {
      int read_result =
          socket->Read(read_buffer.get(), read_buffer->BytesRemaining(),
                       read_callback.callback());
      read_result = read_callback.GetResult(read_result);
      ASSERT_GT(read_result, 0);
      bytes_read += read_result;
      ASSERT_LE(bytes_read, request_len);
      read_buffer->DidConsume(bytes_read);
    }
    ASSERT_EQ(request_len, bytes_read);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<base::RunLoop> run_loop_;

  base::Thread file_thread_;

  // Object under test.
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler_;

  SecurityKeyAuthHandler::SendMessageCallback send_message_callback_;

  int last_connection_id_received_ = -1;
  std::string last_message_received_;

  const std::string expected_request_data_;

  const std::string client_response_data_;

  base::ScopedTempDir temp_dir_;
  base::FilePath socket_path_;
};

TEST_F(SecurityKeyAuthHandlerPosixTest, HandleSingleRequest) {
  CreateSocketAndWait();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.
  WriteRequestData(&client_socket);
  WaitForSendMessageToClient();
  CheckHostDataMessage(1);

  // Verify the connection is now valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Verify that completing a request/response cycle didn't close the socket.
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());

  auth_handler_->SendClientResponse(1, client_response_data_);
  WaitForResponseData(&client_socket);

  // Verify that completing a request/response cycle didn't close the socket.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(SecurityKeyAuthHandlerPosixTest, HandleSingleRequestWithEof) {
  CreateSocketAndWait();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request.
  WriteRequestData(&client_socket);
  WaitForSendMessageToClient();

  // Verify the connection is valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());

  net::SocketPosix raw_socket;
  net::SockaddrStorage address;
  ASSERT_TRUE(net::FillUnixAddress(socket_path_.value(), false, &address));
  raw_socket.AdoptConnectedSocket(client_socket.ReleaseConnectedSocket(),
                                  address);

  // Close the write end of the socket.
  ASSERT_EQ(shutdown(raw_socket.socket_fd(), SHUT_WR), 0);

  // Verify that socket has not been closed yet.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());

  // Wait for the response to be received.
  CheckHostDataMessage(1);

  // Verify that socket has not been closed yet.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());

  auth_handler_->SendClientResponse(1, client_response_data_);

  // Verify the connection has been closed and is no longer valid.
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(SecurityKeyAuthHandlerPosixTest, HandleTwoRequests) {
  CreateSocketAndWait();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.
  WriteRequestData(&client_socket);
  WaitForSendMessageToClient();
  CheckHostDataMessage(1);

  // Verify the connection is now valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Send a 'client' response to the socket and verify the data is received.
  auth_handler_->SendClientResponse(1, client_response_data_);
  WaitForResponseData(&client_socket);

  // Verify the connection is still valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Repeat the request/response cycle.
  WriteRequestData(&client_socket);
  WaitForSendMessageToClient();
  CheckHostDataMessage(1);

  // Verify the connection is still valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  auth_handler_->SendClientResponse(1, client_response_data_);
  WaitForResponseData(&client_socket);

  // Verify the connection is still valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Verify that completing two request/response cycles didn't close the
  // socket.
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(SecurityKeyAuthHandlerPosixTest, HandleTwoIndependentRequests) {
  CreateSocketAndWait();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;

  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.
  WriteRequestData(&client_socket);
  WaitForSendMessageToClient();
  CheckHostDataMessage(1);

  // Verify the first connection is now valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Send a 'client' response to the socket and verify the data is received.
  auth_handler_->SendClientResponse(1, client_response_data_);
  WaitForResponseData(&client_socket);

  // Verify the connection is still valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));

  // Disconnect and establish a new connection.
  client_socket.Disconnect();

  net::TestCompletionCallback connect_callback2;
  rv = client_socket.Connect(connect_callback2.callback());
  ASSERT_EQ(net::OK, connect_callback2.GetResult(rv));

  // Repeat the request/response cycle.
  WriteRequestData(&client_socket);
  WaitForSendMessageToClient();
  CheckHostDataMessage(2);

  // Verify the connection is now valid.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(2));

  // Send a 'client' response to the socket and verify the data is received.
  auth_handler_->SendClientResponse(2, client_response_data_);
  WaitForResponseData(&client_socket);

  // Verify the second connection is valid and the first is not.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(2));
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(SecurityKeyAuthHandlerPosixTest, HandleReadTimeout) {
  CreateSocketAndWait();

  auth_handler_->SetRequestTimeoutForTest(base::TimeDelta());

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;
  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // SSH Error should be received when the connection times out.
  WaitForErrorData(&client_socket);

  // Connection should no longer be valid.
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(SecurityKeyAuthHandlerPosixTest, HandleClientErrorMessage) {
  CreateSocketAndWait();

  net::UnixDomainClientSocket client_socket(socket_path_.value(), false);
  net::TestCompletionCallback connect_callback;
  int rv = client_socket.Connect(connect_callback.callback());
  ASSERT_EQ(net::OK, connect_callback.GetResult(rv));

  // Write the request and verify the response.  This ensures the socket has
  // been created and is working before sending the error to tear it down.
  WriteRequestData(&client_socket);
  WaitForSendMessageToClient();
  CheckHostDataMessage(1);

  // Send a 'client' response to the socket and verify the data is received.
  auth_handler_->SendClientResponse(1, client_response_data_);
  WaitForResponseData(&client_socket);

  ASSERT_TRUE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(1u, auth_handler_->GetActiveConnectionCountForTest());

  auth_handler_->SendErrorAndCloseConnection(1);

  // Connection should be removed immediately.
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(1));
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());

  // SSH Error should be received.
  WaitForErrorData(&client_socket);
}

}  // namespace remoting
