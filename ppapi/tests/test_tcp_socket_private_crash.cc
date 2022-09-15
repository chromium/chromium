// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_socket_private_crash.h"

#include <stddef.h>
#include <stdlib.h>

#include <new>

#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(TCPSocketPrivateCrash);

TestTCPSocketPrivateCrash::TestTCPSocketPrivateCrash(TestingInstance* instance)
    : TestCase(instance) {}

bool TestTCPSocketPrivateCrash::Init() {
  return pp::TCPSocketPrivate::IsAvailable();
}

void TestTCPSocketPrivateCrash::RunTests(const std::string& filter) {
  // No need to run this test with the various callback types since that's
  // orthogonal from the functionality being tested. It would also make the
  // test more complicated because it would have to keep watching the network
  // process restart and telling it to crash again on crash.com.
  RUN_TEST(Resolve, filter);
}

std::string TestTCPSocketPrivateCrash::TestResolve() {
  pp::TCPSocketPrivate socket(instance_);

  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  std::string host("crash.com");
  cb.WaitForResult(socket.Connect(host.c_str(), 80, cb.GetCallback()));
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  socket.Disconnect();

  PASS();
}
