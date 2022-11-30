// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_EMPTY_H_
#define PPAPI_TESTS_TEST_EMPTY_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestEmpty : public TestCase {
 public:
  explicit TestEmpty(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestNaClLoad();
};

#endif  // PPAPI_TESTS_TEST_EMPTY_H_
