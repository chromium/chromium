// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_HOST_RESOLVER_CRASH_H_
#define PPAPI_TESTS_TEST_HOST_RESOLVER_CRASH_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

class TestHostResolverCrash : public TestCase {
 public:
  explicit TestHostResolverCrash(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestBasic();
};

#endif  // PPAPI_TESTS_TEST_HOST_RESOLVER_CRASH_H_
