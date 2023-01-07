// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CORE_H_
#define PPAPI_TESTS_TEST_CORE_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestCore : public TestCase {
 public:
  explicit TestCore(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestTime();
  std::string TestTimeTicks();
};

#endif  // PPAPI_TESTS_TEST_CORE_H_
