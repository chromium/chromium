// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_CRASH_H_
#define PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_CRASH_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

class TestTCPSocketPrivateCrash : public TestCase {
 public:
  explicit TestTCPSocketPrivateCrash(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestResolve();
};

#endif  // PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_CRASH_H_
