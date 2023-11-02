// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_DISALLOWED_H_
#define PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_DISALLOWED_H_

#include <stdint.h>

#include <string>

#include "ppapi/cpp/private/host_resolver_private.h"
#include "ppapi/tests/test_case.h"

class TestHostResolverPrivateDisallowed : public TestCase {
 public:
  explicit TestHostResolverPrivateDisallowed(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestResolve();

  std::string host_;
  uint16_t port_;
};

#endif  // PPAPI_TESTS_TEST_HOST_RESOLVER_PRIVATE_DISALLOWED_H_
