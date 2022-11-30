// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_NETWORK_PROXY_H_
#define PPAPI_TESTS_TEST_NETWORK_PROXY_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestNetworkProxy : public TestCase {
 public:
  explicit TestNetworkProxy(TestingInstance* instance);

 private:
  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

  std::string TestGetProxyForURL();
};

#endif  // PPAPI_TESTS_TEST_NETWORK_PROXY_H_
