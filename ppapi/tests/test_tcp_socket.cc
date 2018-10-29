// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_socket.h"

#include <vector>

#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

namespace {

// Validates the first line of an HTTP response.
bool ValidateHttpResponse(const std::string& s) {
  // Just check that it begins with "HTTP/" and ends with a "\r\n".
  return s.size() >= 5 &&
         s.substr(0, 5) == "HTTP/" &&
         s.substr(s.size() - 2) == "\r\n";
}

}  // namespace

REGISTER_TEST_CASE(TCPSocket);

TestTCPSocket::TestTCPSocket(TestingInstance* instance)
    : TestCase(instance),
      socket_interface_1_0_(NULL) {
}

bool TestTCPSocket::Init() {
  if (!pp::TCPSocket::IsAvailable())
    return false;
  socket_interface_1_0_ =
      static_cast<const PPB_TCPSocket_1_0*>(
          pp::Module::Get()->GetBrowserInterface(PPB_TCPSOCKET_INTERFACE_1_0));
  if (!socket_interface_1_0_)
    return false;

  // We need something to connect to, so we connect to the HTTP server whence we
  // came. Grab the host and port.
  if (!EnsureRunningOverHTTP())
    return false;

  std::string host;
  uint16_t port = 0;
  if (!GetLocalHostPort(instance_->pp_instance(), &host, &port))
    return false;

  if (!ResolveHost(instance_->pp_instance(), host, port, &test_server_addr_))
    return false;

  return true;
}

void TestTCPSocket::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestTCPSocket, Connect, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, ReadWrite, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, SetOption, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, Listen, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, Backlog, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, Interface_1_0, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, UnexpectedCalls, filter);

  RUN_CALLBACK_TEST(TestTCPSocket, ConnectFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, ConnectHangs, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, WriteFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, ReadFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, SetSendBufferSizeFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, SetReceiveBufferSizeFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, SetNoDelayFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, BindFailsConnectSucceeds, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, BindFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, BindHangs, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, ListenFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, ListenHangs, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, AcceptFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, AcceptHangs, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, AcceptedSocketWriteFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, AcceptedSocketReadFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, BindConnectFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, BindConnectHangs, filter);
}

std::string TestTCPSocket::TestConnect() {
  {
    // The basic case.
    pp::TCPSocket socket(instance_);
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());

    cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_OK, cb.result());

    pp::NetAddress local_addr, remote_addr;
    local_addr = socket.GetLocalAddress();
    remote_addr = socket.GetRemoteAddress();

    ASSERT_NE(0, local_addr.pp_resource());
    ASSERT_NE(0, remote_addr.pp_resource());
    ASSERT_TRUE(EqualNetAddress(test_server_addr_, remote_addr));

    socket.Close();
  }

  {
    // Connect a bound socket.
    pp::TCPSocket socket(instance_);
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());

    pp::NetAddress any_port_address;
    ASSERT_SUBTEST_SUCCESS(GetAddressToBind(&any_port_address));

    cb.WaitForResult(socket.Bind(any_port_address, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_OK, cb.result());

    cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_OK, cb.result());

    pp::NetAddress local_addr, remote_addr;
    local_addr = socket.GetLocalAddress();
    remote_addr = socket.GetRemoteAddress();

    ASSERT_NE(0, local_addr.pp_resource());
    ASSERT_NE(0, remote_addr.pp_resource());
    ASSERT_TRUE(EqualNetAddress(test_server_addr_, remote_addr));
    ASSERT_NE(0u, GetPort(local_addr));

    socket.Close();
  }

  PASS();
}

std::string TestTCPSocket::TestReadWrite() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  ASSERT_SUBTEST_SUCCESS(WriteToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));

  // Read up to the first \n and check that it looks like valid HTTP response.
  std::string s;
  ASSERT_SUBTEST_SUCCESS(ReadFirstLineFromSocket(&socket, &s));
  ASSERT_TRUE(ValidateHttpResponse(s));

  // Read until the server closes the socket.
  std::string read_data;
  int read_error;
  ASSERT_SUBTEST_SUCCESS(
      ReadFromSocketUntilError(&socket, &read_data, &read_error));
  ASSERT_EQ(PP_OK, read_error);

  // Reading again from the socket after getting an EOF should result in an
  // error.
  read_data = "";
  ASSERT_SUBTEST_SUCCESS(
      ReadFromSocketUntilError(&socket, &read_data, &read_error));
  ASSERT_EQ(PP_ERROR_FAILED, read_error);
  ASSERT_EQ("", read_data);

  char write_data[32 * 1024] = {0};
  // Write to the socket until there's an error, just to make sure the error
  // handling code works. As with the read case, go through two failures
  // (which may or may not fail with the same error code).
  int failures = 0;
  while (true) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket.Write(write_data,
                                  static_cast<int32_t>(sizeof(write_data)),
                                  cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    if (cb.result() > 0) {
      ASSERT_EQ(0, failures);
      continue;
    }
    // While this will most likely be PP_ERROR_CONNECTION_ABORTED, it seems best
    // not to rely on that, as write errors can be a bit finicky.
    ASSERT_LT(cb.result(), 0);
    ASSERT_NE(PP_ERROR_FAILED, cb.result());
    ASSERT_NE(PP_OK_COMPLETIONPENDING, cb.result());
    ++failures;

    if (failures == 2)
      break;
  }

  PASS();
}

std::string TestTCPSocket::TestSetOption() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb_1(instance_->pp_instance(), callback_type());
  TestCompletionCallback cb_2(instance_->pp_instance(), callback_type());
  TestCompletionCallback cb_3(instance_->pp_instance(), callback_type());

  // These options can be set even before the socket is connected.
  int32_t result_1 = socket.SetOption(PP_TCPSOCKET_OPTION_NO_DELAY,
                                      true, cb_1.GetCallback());
  int32_t result_2 = socket.SetOption(PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE,
                                      256, cb_2.GetCallback());
  int32_t result_3 = socket.SetOption(PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE,
                                      512, cb_3.GetCallback());

  cb_1.WaitForResult(result_1);
  CHECK_CALLBACK_BEHAVIOR(cb_1);
  ASSERT_EQ(PP_OK, cb_1.result());

  cb_2.WaitForResult(result_2);
  CHECK_CALLBACK_BEHAVIOR(cb_2);
  ASSERT_EQ(PP_OK, cb_2.result());

  cb_3.WaitForResult(result_3);
  CHECK_CALLBACK_BEHAVIOR(cb_3);
  ASSERT_EQ(PP_OK, cb_3.result());

  cb_1.WaitForResult(socket.Connect(test_server_addr_, cb_1.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb_1);
  ASSERT_EQ(PP_OK, cb_1.result());

  result_1 = socket.SetOption(PP_TCPSOCKET_OPTION_NO_DELAY,
                              false, cb_1.GetCallback());
  result_2 = socket.SetOption(PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE,
                              512, cb_2.GetCallback());
  result_3 = socket.SetOption(PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE,
                              1024, cb_3.GetCallback());

  cb_1.WaitForResult(result_1);
  CHECK_CALLBACK_BEHAVIOR(cb_1);
  ASSERT_EQ(PP_OK, cb_1.result());

  cb_2.WaitForResult(result_2);
  CHECK_CALLBACK_BEHAVIOR(cb_2);
  ASSERT_EQ(PP_OK, cb_2.result());

  cb_3.WaitForResult(result_3);
  CHECK_CALLBACK_BEHAVIOR(cb_3);
  ASSERT_EQ(PP_OK, cb_3.result());

  PASS();
}

std::string TestTCPSocket::TestListen() {
  // TODO(mmenke): Whenever this test is run, the PPAPI process DCHECKs on
  // shutdown when a ref count is decremented on the wrong thread. Someone
  // should probably look into that.
  static const int kBacklog = 2;

  pp::TCPSocket server_socket(instance_);
  ASSERT_SUBTEST_SUCCESS(StartListen(&server_socket, kBacklog));

  // We can't use a blocking callback for Accept, because it will wait forever
  // for the client to connect, since the client connects after.
  TestCompletionCallbackWithOutput<pp::TCPSocket>
      accept_callback(instance_->pp_instance(), PP_REQUIRED);
  // We need to make sure there's a message loop to run accept_callback on.
  pp::MessageLoop current_thread_loop(pp::MessageLoop::GetCurrent());
  if (current_thread_loop.is_null() && testing_interface_->IsOutOfProcess()) {
    current_thread_loop = pp::MessageLoop(instance_);
    current_thread_loop.AttachToCurrentThread();
  }

  int32_t accept_rv = server_socket.Accept(accept_callback.GetCallback());

  pp::TCPSocket client_socket;
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  do {
    client_socket = pp::TCPSocket(instance_);

    callback.WaitForResult(client_socket.Connect(
        server_socket.GetLocalAddress(), callback.GetCallback()));
  } while (callback.result() != PP_OK);

  pp::NetAddress client_local_addr = client_socket.GetLocalAddress();
  pp::NetAddress client_remote_addr = client_socket.GetRemoteAddress();
  ASSERT_FALSE(client_local_addr.is_null());
  ASSERT_FALSE(client_remote_addr.is_null());

  accept_callback.WaitForResult(accept_rv);
  CHECK_CALLBACK_BEHAVIOR(accept_callback);
  ASSERT_EQ(PP_OK, accept_callback.result());

  pp::TCPSocket accepted_socket(accept_callback.output());
  pp::NetAddress accepted_local_addr = accepted_socket.GetLocalAddress();
  pp::NetAddress accepted_remote_addr = accepted_socket.GetRemoteAddress();
  ASSERT_FALSE(accepted_local_addr.is_null());
  ASSERT_FALSE(accepted_remote_addr.is_null());

  ASSERT_TRUE(EqualNetAddress(client_local_addr, accepted_remote_addr));

  const std::string kSentData = "a";
  ASSERT_SUBTEST_SUCCESS(WriteToSocket(&client_socket, kSentData));

  // Close the client socket to be able to read until EOF.
  client_socket.Close();

  std::string read_data;
  int read_error;
  ASSERT_SUBTEST_SUCCESS(
      ReadFromSocketUntilError(&accepted_socket, &read_data, &read_error));
  ASSERT_EQ(kSentData, read_data);
  ASSERT_EQ(PP_OK, read_error);

  accepted_socket.Close();
  server_socket.Close();

  PASS();
}

std::string TestTCPSocket::TestBacklog() {
  static const size_t kBacklog = 5;

  pp::TCPSocket server_socket(instance_);
  ASSERT_SUBTEST_SUCCESS(StartListen(&server_socket, 2 * kBacklog));

  std::vector<pp::TCPSocket*> client_sockets(kBacklog);
  std::vector<TestCompletionCallback*> connect_callbacks(kBacklog);
  std::vector<int32_t> connect_rv(kBacklog);
  pp::NetAddress address = server_socket.GetLocalAddress();
  for (size_t i = 0; i < kBacklog; ++i) {
    client_sockets[i] = new pp::TCPSocket(instance_);
    connect_callbacks[i] = new TestCompletionCallback(instance_->pp_instance(),
                                                      callback_type());
    connect_rv[i] = client_sockets[i]->Connect(
        address, connect_callbacks[i]->GetCallback());
  }

  std::vector<pp::TCPSocket*> accepted_sockets(kBacklog);
  for (size_t i = 0; i < kBacklog; ++i) {
    TestCompletionCallbackWithOutput<pp::TCPSocket> callback(
        instance_->pp_instance(), callback_type());
    callback.WaitForResult(server_socket.Accept(callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());

    accepted_sockets[i] = new pp::TCPSocket(callback.output());
    ASSERT_FALSE(accepted_sockets[i]->is_null());
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    connect_callbacks[i]->WaitForResult(connect_rv[i]);
    CHECK_CALLBACK_BEHAVIOR(*connect_callbacks[i]);
    ASSERT_EQ(PP_OK, connect_callbacks[i]->result());
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    const char byte = static_cast<char>('a' + i);
    ASSERT_SUBTEST_SUCCESS(WriteToSocket(client_sockets[i],
                                         std::string(1, byte)));
  }

  bool byte_received[kBacklog] = {};
  for (size_t i = 0; i < kBacklog; ++i) {
    char byte;
    ASSERT_SUBTEST_SUCCESS(ReadFromSocket(
        accepted_sockets[i], &byte, sizeof(byte)));
    const size_t index = byte - 'a';
    ASSERT_GE(index, 0u);
    ASSERT_LT(index, kBacklog);
    ASSERT_FALSE(byte_received[index]);
    byte_received[index] = true;
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    ASSERT_TRUE(byte_received[i]);

    delete client_sockets[i];
    delete connect_callbacks[i];
    delete accepted_sockets[i];
  }

  PASS();
}

std::string TestTCPSocket::TestInterface_1_0() {
  PP_Resource socket = socket_interface_1_0_->Create(instance_->pp_instance());
  ASSERT_NE(0, socket);

  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(socket_interface_1_0_->Connect(
      socket, test_server_addr_.pp_resource(),
      cb.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  ASSERT_SUBTEST_SUCCESS(WriteToSocket_1_0(socket, "GET / HTTP/1.0\r\n\r\n"));

  // Read up to the first \n and check that it looks like valid HTTP response.
  std::string s;
  ASSERT_SUBTEST_SUCCESS(ReadFirstLineFromSocket_1_0(socket, &s));
  ASSERT_TRUE(ValidateHttpResponse(s));

  pp::Module::Get()->core()->ReleaseResource(socket);
  PASS();
}

std::string TestTCPSocket::TestUnexpectedCalls() {
  // Tests that calls that are not expected given a sockets current state fail
  // with PP_ERROR_FAILED without breaking future operations on the socket.

  // Test a listen socket.
  {
    pp::TCPSocket socket(instance_);
    ASSERT_SUBTEST_SUCCESS(
        RunCommandsExpendingFailures(&socket, kListen | kAccept | kReadWrite));

    // Connect
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_OK, cb.result());
    ASSERT_SUBTEST_SUCCESS(
        RunCommandsExpendingFailures(&socket, kListen | kAccept | kConnect));

    // Write
    ASSERT_SUBTEST_SUCCESS(WriteToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));
    ASSERT_SUBTEST_SUCCESS(
        RunCommandsExpendingFailures(&socket, kListen | kAccept | kConnect));

    // Read
    std::string s;
    ASSERT_SUBTEST_SUCCESS(ReadFirstLineFromSocket(&socket, &s));
    ASSERT_TRUE(ValidateHttpResponse(s));
    ASSERT_SUBTEST_SUCCESS(
        RunCommandsExpendingFailures(&socket, kListen | kAccept | kConnect));

    socket.Close();
    ASSERT_SUBTEST_SUCCESS(RunCommandsExpendingFailures(
        &socket, kListen | kAccept | kConnect | kReadWrite));
  }

  // Test a server socket.
  {
    pp::TCPSocket server_socket(instance_);

    // Bind
    pp::NetAddress any_port_address;
    ASSERT_SUBTEST_SUCCESS(GetAddressToBind(&any_port_address));
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    callback.WaitForResult(
        server_socket.Bind(any_port_address, callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());
    ASSERT_SUBTEST_SUCCESS(RunCommandsExpendingFailures(
        &server_socket, kBind | kAccept | kReadWrite));

    // Listen
    callback.WaitForResult(
        server_socket.Listen(1 /* backlog */, callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());
    ASSERT_SUBTEST_SUCCESS(RunCommandsExpendingFailures(
        &server_socket, kBind | kListen | kConnect | kReadWrite));

    // Accept
    pp::TCPSocket client_socket(instance_);
    TestCompletionCallback connect_callback(instance_->pp_instance(),
                                            callback_type());
    int connect_result = client_socket.Connect(server_socket.GetLocalAddress(),
                                               connect_callback.GetCallback());
    TestCompletionCallbackWithOutput<pp::TCPSocket> accept_callback(
        instance_->pp_instance(), callback_type());
    accept_callback.WaitForResult(
        server_socket.Accept(accept_callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(accept_callback);
    ASSERT_EQ(PP_OK, accept_callback.result());
    connect_callback.WaitForResult(connect_result);
    CHECK_CALLBACK_BEHAVIOR(connect_callback);
    ASSERT_EQ(PP_OK, connect_callback.result());
    pp::TCPSocket accepted_socket(accept_callback.output());
    ASSERT_SUBTEST_SUCCESS(RunCommandsExpendingFailures(
        &server_socket, kBind | kListen | kConnect | kReadWrite));

    client_socket.Close();
    accepted_socket.Close();
    server_socket.Close();
  }

  PASS();
}

std::string TestTCPSocket::TestConnectFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  // All subsequent calls on the socket should fail.
  ASSERT_SUBTEST_SUCCESS(RunCommandsExpendingFailures(&socket, kAllCommands));

  PASS();
}

std::string TestTCPSocket::TestConnectHangs() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  socket.Connect(test_server_addr_, DoNothingCallback());
  PASS();
}

std::string TestTCPSocket::TestWriteFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  // Write to the socket until there's an error. Some writes may succeed, since
  // Mojo writes complete before the socket tries to send data. As with the read
  // case, wait for two errors.
  char write_data[32 * 1024] = {0};
  int failures = 0;
  while (true) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket.Write(write_data,
                                  static_cast<int32_t>(sizeof(write_data)),
                                  cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    if (cb.result() > 0) {
      ASSERT_EQ(0, failures);
      continue;
    }

    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
    ++failures;

    if (failures == 2)
      break;
  }

  PASS();
}

std::string TestTCPSocket::TestReadFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  std::string read_data;
  int read_error;
  // Read should fail with no data received.
  ASSERT_SUBTEST_SUCCESS(
      ReadFromSocketUntilError(&socket, &read_data, &read_error));
  ASSERT_EQ(PP_ERROR_FAILED, read_error);
  ASSERT_EQ(0, read_data.size());

  // Reading again after the socket has been closed should also fail.
  ASSERT_SUBTEST_SUCCESS(
      ReadFromSocketUntilError(&socket, &read_data, &read_error));
  ASSERT_EQ(PP_ERROR_FAILED, read_error);
  ASSERT_EQ(0, read_data.size());

  PASS();
}

std::string TestTCPSocket::TestSetSendBufferSizeFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(socket.SetOption(PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE, 256,
                                    cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  PASS();
}

std::string TestTCPSocket::TestSetReceiveBufferSizeFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(socket.SetOption(PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE, 256,
                                    cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  PASS();
}

std::string TestTCPSocket::TestSetNoDelayFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(
      socket.SetOption(PP_TCPSOCKET_OPTION_NO_DELAY, true, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  PASS();
}

std::string TestTCPSocket::TestBindFailsConnectSucceeds() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  // The address doesn't matter here, other than that it should be valid.
  callback.WaitForResult(
      socket.Bind(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  callback.WaitForResult(
      socket.Connect(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  PASS();
}

std::string TestTCPSocket::TestBindFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  // The address doesn't matter here, other than that it should be valid.
  callback.WaitForResult(
      socket.Bind(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  PASS();
}

std::string TestTCPSocket::TestBindHangs() {
  pp::TCPSocket socket(instance_);
  socket.Bind(test_server_addr_, DoNothingCallback());
  PASS();
}

std::string TestTCPSocket::TestListenFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  // The address doesn't matter here, other than that it should be valid.
  callback.WaitForResult(
      socket.Bind(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(
      socket.Listen(2 /* backlog */, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  // All subsequent calls on the socket should fail.
  ASSERT_SUBTEST_SUCCESS(RunCommandsExpendingFailures(&socket, kAllCommands));

  PASS();
}

std::string TestTCPSocket::TestListenHangs() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  // The address doesn't matter here, other than that it should be valid.
  callback.WaitForResult(
      socket.Bind(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  socket.Listen(2 /* backlog */, DoNothingCallback());
  PASS();
}

std::string TestTCPSocket::TestAcceptFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  // The address doesn't matter here, other than that it should be valid.
  callback.WaitForResult(
      socket.Bind(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(
      socket.Listen(2 /* backlog */, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  TestCompletionCallbackWithOutput<pp::TCPSocket> accept_callback(
      instance_->pp_instance(), callback_type());
  accept_callback.WaitForResult(socket.Accept(accept_callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(accept_callback);
  ASSERT_EQ(PP_ERROR_FAILED, accept_callback.result());

  PASS();
}

std::string TestTCPSocket::TestAcceptHangs() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  // The address doesn't matter here, other than that it should be valid.
  callback.WaitForResult(
      socket.Bind(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(
      socket.Listen(2 /* backlog */, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  socket.Accept(DoNothingCallbackWithOutput<pp::TCPSocket>());
  PASS();
}

std::string TestTCPSocket::TestAcceptedSocketWriteFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  // The address doesn't matter here, other than that it should be valid.
  callback.WaitForResult(
      socket.Bind(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(
      socket.Listen(2 /* backlog */, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  TestCompletionCallbackWithOutput<pp::TCPSocket> accept_callback(
      instance_->pp_instance(), callback_type());
  accept_callback.WaitForResult(socket.Accept(accept_callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(accept_callback);
  ASSERT_EQ(PP_OK, accept_callback.result());

  pp::TCPSocket accepted_socket(accept_callback.output());

  // Write to the socket until there's an error. Some writes may succeed, since
  // Mojo writes complete before the socket tries to send data. As with the read
  // case, wait for two errors.
  char write_data[32 * 1024] = {0};
  int failures = 0;
  while (true) {
    callback.WaitForResult(accepted_socket.Write(
        write_data, static_cast<int32_t>(sizeof(write_data)),
        callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    if (callback.result() > 0) {
      ASSERT_EQ(0, failures);
      continue;
    }

    ASSERT_EQ(PP_ERROR_FAILED, callback.result());
    ++failures;

    if (failures == 2)
      break;
  }

  PASS();
}

std::string TestTCPSocket::TestAcceptedSocketReadFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  // The address doesn't matter here, other than that it should be valid.
  callback.WaitForResult(
      socket.Bind(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(
      socket.Listen(2 /* backlog */, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  TestCompletionCallbackWithOutput<pp::TCPSocket> accept_callback(
      instance_->pp_instance(), callback_type());
  accept_callback.WaitForResult(socket.Accept(accept_callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(accept_callback);
  ASSERT_EQ(PP_OK, accept_callback.result());

  pp::TCPSocket accepted_socket(accept_callback.output());

  std::string read_data;
  int read_error;
  // Read should fail with no data received.
  ASSERT_SUBTEST_SUCCESS(
      ReadFromSocketUntilError(&accepted_socket, &read_data, &read_error));
  ASSERT_EQ(PP_ERROR_FAILED, read_error);
  ASSERT_EQ(0, read_data.size());

  // Reading again after the socket has been closed should also fail.
  ASSERT_SUBTEST_SUCCESS(
      ReadFromSocketUntilError(&accepted_socket, &read_data, &read_error));
  ASSERT_EQ(PP_ERROR_FAILED, read_error);
  ASSERT_EQ(0, read_data.size());

  PASS();
}

std::string TestTCPSocket::TestBindConnectFails() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Bind(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(socket.Connect(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  // All subsequent calls on the socket should fail.
  ASSERT_SUBTEST_SUCCESS(RunCommandsExpendingFailures(&socket, kAllCommands));

  PASS();
}

std::string TestTCPSocket::TestBindConnectHangs() {
  pp::TCPSocket socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Bind(test_server_addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  socket.Connect(test_server_addr_, DoNothingCallback());
  PASS();
}

std::string TestTCPSocket::ReadFirstLineFromSocket(pp::TCPSocket* socket,
                                                   std::string* s) {
  char buffer[1000];

  s->clear();
  // Make sure we don't just hang if |Read()| spews.
  while (s->size() < 10000) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket->Read(buffer, sizeof(buffer), cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_GT(cb.result(), 0);
    s->reserve(s->size() + cb.result());
    for (int32_t i = 0; i < cb.result(); ++i) {
      s->push_back(buffer[i]);
      if (buffer[i] == '\n')
        PASS();
    }
  }
  PASS();
}

std::string TestTCPSocket::ReadFirstLineFromSocket_1_0(PP_Resource socket,
                                                       std::string* s) {
  char buffer[1000];

  s->clear();
  // Make sure we don't just hang if |Read()| spews.
  while (s->size() < 10000) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket_interface_1_0_->Read(
        socket, buffer, sizeof(buffer),
        cb.GetCallback().pp_completion_callback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_GT(cb.result(), 0);
    s->reserve(s->size() + cb.result());
    for (int32_t i = 0; i < cb.result(); ++i) {
      s->push_back(buffer[i]);
      if (buffer[i] == '\n')
        PASS();
    }
  }
  PASS();
}

std::string TestTCPSocket::ReadFromSocket(pp::TCPSocket* socket,
                                          char* buffer,
                                          size_t num_bytes) {
  while (num_bytes > 0) {
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    callback.WaitForResult(
        socket->Read(buffer, static_cast<int32_t>(num_bytes),
        callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_GT(callback.result(), 0);
    buffer += callback.result();
    num_bytes -= callback.result();
  }
  ASSERT_EQ(0u, num_bytes);
  PASS();
}

// Reads from the socket until an error (Or 0-byte read) occurs. Populates
// |read_data| and |error| with that information. Doesn't return a std::string
// to distinguish it from a subtest.
std::string TestTCPSocket::ReadFromSocketUntilError(pp::TCPSocket* socket,
                                                    std::string* read_data,
                                                    int* read_error) {
  // Set |read_error| to a value that a read should never complete with.
  *read_error = PP_OK_COMPLETIONPENDING;
  while (true) {
    char buffer[1024];
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    callback.WaitForResult(
        socket->Read(buffer, sizeof(buffer), callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_LE(callback.result(), static_cast<int32_t>(sizeof(buffer)));
    ASSERT_NE(PP_OK_COMPLETIONPENDING, callback.result());
    if (callback.result() <= 0) {
      *read_error = callback.result();
      PASS();
    }
    read_data->append(buffer, callback.result());
  }
}

std::string TestTCPSocket::WriteToSocket(pp::TCPSocket* socket,
                                         const std::string& s) {
  const char* buffer = s.data();
  size_t written = 0;
  while (written < s.size()) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(
        socket->Write(buffer + written,
                      static_cast<int32_t>(s.size() - written),
                      cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_GT(cb.result(), 0);
    written += cb.result();
  }
  ASSERT_EQ(written, s.size());
  PASS();
}

std::string TestTCPSocket::WriteToSocket_1_0(
    PP_Resource socket,
    const std::string& s) {
  const char* buffer = s.data();
  size_t written = 0;
  while (written < s.size()) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket_interface_1_0_->Write(
        socket, buffer + written,
        static_cast<int32_t>(s.size() - written),
        cb.GetCallback().pp_completion_callback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_GT(cb.result(), 0);
    written += cb.result();
  }
  ASSERT_EQ(written, s.size());
  PASS();
}

std::string TestTCPSocket::GetAddressToBind(pp::NetAddress* address) {
  // Connect to |test_server_addr_| and then sets |address| to the local address
  // used by that connection, replacing the port with 0 to use a random port.
  pp::TCPSocket socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(
      socket.Connect(test_server_addr_, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  ASSERT_TRUE(ReplacePort(instance_->pp_instance(), socket.GetLocalAddress(), 0,
                          address));
  ASSERT_FALSE(address->is_null());
  PASS();
}

std::string TestTCPSocket::StartListen(pp::TCPSocket* socket, int32_t backlog) {
  pp::NetAddress any_port_address;
  ASSERT_SUBTEST_SUCCESS(GetAddressToBind(&any_port_address));

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(
      socket->Bind(any_port_address, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  callback.WaitForResult(
      socket->Listen(backlog, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  PASS();
}

std::string TestTCPSocket::RunCommandsExpendingFailures(pp::TCPSocket* socket,
                                                        int commands) {
  ASSERT_NE(0, commands);

  if (commands & kBind) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    pp::NetAddress any_port_address;
    ASSERT_TRUE(ReplacePort(instance_->pp_instance(), test_server_addr_, 0,
                            &any_port_address));
    cb.WaitForResult(socket->Bind(any_port_address, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  }

  if (commands & kListen) {
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    callback.WaitForResult(
        socket->Listen(1 /* backlog */, callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_ERROR_FAILED, callback.result());
  }

  if (commands & kAccept) {
    TestCompletionCallbackWithOutput<pp::TCPSocket> accept_callback(
        instance_->pp_instance(), callback_type());
    accept_callback.WaitForResult(
        socket->Accept(accept_callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(accept_callback);
    ASSERT_EQ(PP_ERROR_FAILED, accept_callback.result());
  }

  if (commands & kConnect) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket->Connect(test_server_addr_, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  }

  if (commands & kReadWrite) {
    // Check that a read on a new socket fails.
    char buffer[1] = {'1'};
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket->Read(buffer, sizeof(buffer), cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_ERROR_FAILED, cb.result());

    // Check that a write on a new socket fails.
    cb.WaitForResult(socket->Write(buffer, sizeof(buffer), cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  }

  PASS();
}
