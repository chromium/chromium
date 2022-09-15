// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_DISALLOWED_H_
#define PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_DISALLOWED_H_

#include <string>

#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/tests/test_case.h"

class TestTCPSocketPrivateDisallowed : public TestCase {
 public:
  explicit TestTCPSocketPrivateDisallowed(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestConnect();

  const PPB_TCPSocket_Private* tcp_socket_private_interface_;
};

#endif  // PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_DISALLOWED_H_
