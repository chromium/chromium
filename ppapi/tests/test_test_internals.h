// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TEST_INTERNALS_H_
#define PPAPI_TESTS_TEST_TEST_INTERNALS_H_

#include <string>

#include "ppapi/tests/test_case.h"

// This class is for testing the test framework itself.
class TestTestInternals : public TestCase {
 public:
  explicit TestTestInternals(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestToString();
  std::string TestPassingComparisons();
  std::string TestFailingComparisons();
  std::string TestEvaluateOnce();
};

#endif  // PPAPI_TESTS_TEST_TEST_INTERNALS_H_
