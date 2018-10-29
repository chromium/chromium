// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_TCP_SOCKET_PRIVATE_H_
#define PAPPI_TESTS_TEST_TCP_SOCKET_PRIVATE_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

namespace pp {
class TCPSocketPrivate;
}

class TestTCPSocketPrivate : public TestCase {
 public:
  explicit TestTCPSocketPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestBasic();
  std::string TestReadWrite();
  std::string TestReadWriteSSL();
  std::string TestConnectAddress();
  std::string TestSetOption();
  std::string TestLargeRead();

  // The higher level test fixture is responsible for making socket methods
  // behave in the expected manner.  The *Fails tests expect the specified event
  // to fail with PP_ERROR_FAILED, and the *Hangs test expects the specified
  // operation to never complete, at least until teardown starts.
  std::string TestSSLHandshakeFails();
  std::string TestSSLHandshakeHangs();
  std::string TestSSLWriteFails();
  std::string TestSSLReadFails();

  int32_t ReadFirstLineFromSocket(pp::TCPSocketPrivate* socket, std::string* s);
  int32_t WriteStringToSocket(pp::TCPSocketPrivate* socket,
                              const std::string& s);

  std::string host_;
  uint16_t port_;
  uint16_t ssl_port_;
};

#endif  // PAPPI_TESTS_TEST_TCP_SOCKET_PRIVATE_H_
