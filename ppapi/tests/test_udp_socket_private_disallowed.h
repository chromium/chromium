// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_DISALLOWED_H_
#define PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_DISALLOWED_H_

#include <string>

#include "ppapi/cpp/private/udp_socket_private.h"
#include "ppapi/tests/test_case.h"

class TestUDPSocketPrivateDisallowed : public TestCase {
 public:
  explicit TestUDPSocketPrivateDisallowed(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestBind();

  const PPB_UDPSocket_Private* udp_socket_private_interface_;
};

#endif  // PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_DISALLOWED_H_
