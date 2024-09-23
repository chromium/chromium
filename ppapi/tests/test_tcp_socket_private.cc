// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_tcp_socket_private.h"

#include <stddef.h>
#include <stdlib.h>

#include <new>

#include "ppapi/cpp/private/tcp_socket_private.h"
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

REGISTER_TEST_CASE(TCPSocketPrivate);

TestTCPSocketPrivate::TestTCPSocketPrivate(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestTCPSocketPrivate::Init() {
  if (!pp::TCPSocketPrivate::IsAvailable())
    return false;

  // We need something to connect to, so we connect to the HTTP server whence we
  // came. Grab the host and port.
  if (!EnsureRunningOverHTTP())
    return false;

  if (!GetLocalHostPort(instance_->pp_instance(), &host_, &port_))
    return false;

  // Get the port for the SSL server.
  ssl_port_ = instance_->ssl_server_port();

  return true;
}

void TestTCPSocketPrivate::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, Basic, filter);
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, ReadWrite, filter);
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, ReadWriteSSL, filter);
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, ConnectAddress, filter);
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, SetOption, filter);
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, LargeRead, filter);

  RUN_CALLBACK_TEST(TestTCPSocketPrivate, SSLHandshakeFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, SSLHandshakeHangs, filter);
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, SSLWriteFails, filter);
  RUN_CALLBACK_TEST(TestTCPSocketPrivate, SSLReadFails, filter);
}

std::string TestTCPSocketPrivate::TestBasic() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect(host_.c_str(), port_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  PP_NetAddress_Private unused;
  // TODO(viettrungluu): check the values somehow.
  ASSERT_TRUE(socket.GetLocalAddress(&unused));
  ASSERT_TRUE(socket.GetRemoteAddress(&unused));

  socket.Disconnect();

  PASS();
}

std::string TestTCPSocketPrivate::TestReadWrite() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect(host_.c_str(), port_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  ASSERT_EQ(PP_OK, WriteStringToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));

  // Read up to the first \n and check that it looks like valid HTTP response.
  std::string s;
  ASSERT_EQ(PP_OK, ReadFirstLineFromSocket(&socket, &s));
  ASSERT_TRUE(ValidateHttpResponse(s));

  socket.Disconnect();

  PASS();
}

std::string TestTCPSocketPrivate::TestReadWriteSSL() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect(host_.c_str(), ssl_port_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(
      socket.SSLHandshake(host_.c_str(), ssl_port_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  ASSERT_EQ(PP_OK, WriteStringToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));

  // Read up to the first \n and check that it looks like valid HTTP response.
  std::string s;
  ASSERT_EQ(PP_OK, ReadFirstLineFromSocket(&socket, &s));
  ASSERT_TRUE(ValidateHttpResponse(s));

  socket.Disconnect();

  PASS();
}

std::string TestTCPSocketPrivate::TestConnectAddress() {
  PP_NetAddress_Private address;

  // First, bring up a connection and grab the address.
  {
    pp::TCPSocketPrivate socket(instance_);
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket.Connect(host_.c_str(), port_, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_OK, cb.result());
    ASSERT_TRUE(socket.GetRemoteAddress(&address));
    // Omit the |Disconnect()| here to make sure we don't crash if we just let
    // the resource be destroyed.
  }

  // Connect to that address.
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(socket.ConnectWithNetAddress(&address, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  // Make sure we can read/write to it properly (see |TestReadWrite()|).
  ASSERT_EQ(PP_OK, WriteStringToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));
  std::string s;
  ASSERT_EQ(PP_OK, ReadFirstLineFromSocket(&socket, &s));
  ASSERT_TRUE(ValidateHttpResponse(s));

  socket.Disconnect();

  PASS();
}

std::string TestTCPSocketPrivate::TestSetOption() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(
      socket.SetOption(PP_TCPSOCKETOPTION_PRIVATE_NO_DELAY, true,
                       cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  cb.WaitForResult(socket.Connect(host_.c_str(), port_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(
      socket.SetOption(PP_TCPSOCKETOPTION_PRIVATE_NO_DELAY, true,
                       cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(
      socket.SetOption(PP_TCPSOCKETOPTION_PRIVATE_INVALID, true,
                       cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, cb.result());

  socket.Disconnect();

  PASS();
}

std::string TestTCPSocketPrivate::TestLargeRead() {
  pp::TCPSocketPrivate socket(instance_);
  {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());

    cb.WaitForResult(socket.Connect(host_.c_str(), port_, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_OK, cb.result());
  }

  ASSERT_EQ(PP_OK, WriteStringToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));

  const size_t kReadSize = 1024 * 1024 + 32;
  // Create large buffer in heap to prevent run-time errors related to
  // limits on stack size.
  char* buffer = new (std::nothrow) char[kReadSize];
  ASSERT_TRUE(buffer != NULL);

  TestCompletionCallback cb(instance_->pp_instance(), callback_type());
  cb.WaitForResult(socket.Read(buffer, kReadSize * sizeof(*buffer),
                               cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_LE(0, cb.result());

  delete [] buffer;

  PASS();
}

std::string TestTCPSocketPrivate::TestSSLHandshakeFails() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect("foo.test", 443, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(socket.SSLHandshake("foo.test", 443, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  // Writes and reads should both fail after an SSL handshake fails.

  char byte = 'a';
  cb.WaitForResult(socket.Write(&byte, 1, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  cb.WaitForResult(socket.Read(&byte, 1, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  PASS();
}

std::string TestTCPSocketPrivate::TestSSLHandshakeHangs() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect("foo.test", 443, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  socket.SSLHandshake("foo.test", 443, DoNothingCallback());
  PASS();
}

std::string TestTCPSocketPrivate::TestSSLWriteFails() {
  pp::TCPSocketPrivate socket(instance_);
  {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());

    cb.WaitForResult(socket.Connect("foo.test", 443, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_OK, cb.result());

    cb.WaitForResult(socket.SSLHandshake("foo.test", 443, cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    ASSERT_EQ(PP_OK, cb.result());
  }

  // Write to the socket until there's an error. Some writes may succeed, since
  // Mojo writes complete before the socket tries to send data.
  char write_data[32 * 1024] = {0};
  while (true) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    cb.WaitForResult(socket.Write(write_data,
                                  static_cast<int32_t>(sizeof(write_data)),
                                  cb.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cb);
    if (cb.result() > 0)
      continue;

    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
    PASS();
  }
}

std::string TestTCPSocketPrivate::TestSSLReadFails() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect("foo.test", 443, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(socket.SSLHandshake("foo.test", 443, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  char byte;
  cb.WaitForResult(socket.Read(&byte, 1, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  PASS();
}

int32_t TestTCPSocketPrivate::ReadFirstLineFromSocket(
    pp::TCPSocketPrivate* socket,
    std::string* s) {
  char buffer[10000];

  s->clear();
  // Make sure we don't just hang if |Read()| spews.
  while (s->size() < 1000000) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    int32_t rv = socket->Read(buffer, sizeof(buffer), cb.GetCallback());
    if (callback_type() == PP_REQUIRED && rv != PP_OK_COMPLETIONPENDING)
      return PP_ERROR_FAILED;
    cb.WaitForResult(rv);
    if (cb.result() < 0)
      return cb.result();
    if (cb.result() == 0)
      return PP_ERROR_FAILED;  // Didn't get a \n-terminated line.
    s->reserve(s->size() + cb.result());
    for (int32_t i = 0; i < cb.result(); i++) {
      s->push_back(buffer[i]);
      if (buffer[i] == '\n')
        return PP_OK;
    }
  }
  return PP_ERROR_FAILED;
}

int32_t TestTCPSocketPrivate::WriteStringToSocket(pp::TCPSocketPrivate* socket,
                                                  const std::string& s) {
  const char* buffer = s.data();
  size_t written = 0;
  while (written < s.size()) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    int32_t rv = socket->Write(buffer + written,
                               static_cast<int32_t>(s.size() - written),
                               cb.GetCallback());
    if (callback_type() == PP_REQUIRED && rv != PP_OK_COMPLETIONPENDING)
      return PP_ERROR_FAILED;
    cb.WaitForResult(rv);
    if (cb.result() < 0)
      return cb.result();
    if (cb.result() == 0)
      return PP_ERROR_FAILED;
    written += cb.result();
  }
  if (written != s.size())
    return PP_ERROR_FAILED;
  return PP_OK;
}
