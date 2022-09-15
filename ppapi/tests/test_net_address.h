// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_NET_ADDRESS_H_
#define PPAPI_TESTS_TEST_NET_ADDRESS_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestNetAddress : public TestCase {
 public:
  explicit TestNetAddress(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestIPv4Address();
  std::string TestIPv6Address();
  std::string TestDescribeAsString();
};

#endif  // PPAPI_TESTS_TEST_NET_ADDRESS_H_
