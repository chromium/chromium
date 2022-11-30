// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TCP_SERVER_SOCKET_PRIVATE_DISALLOWED_H_
#define PPAPI_TESTS_TEST_TCP_SERVER_SOCKET_PRIVATE_DISALLOWED_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/tests/test_case.h"

class TestTCPServerSocketPrivateDisallowed : public TestCase {
 public:
  explicit TestTCPServerSocketPrivateDisallowed(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestListen();

  const PPB_Core* core_interface_;
  const PPB_TCPServerSocket_Private* tcp_server_socket_private_interface_;
};

#endif  // PPAPI_TESTS_TEST_TCP_SERVER_SOCKET_PRIVATE_DISALLOWED_H_
