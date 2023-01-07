// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UMA_H_
#define PPAPI_TESTS_TEST_UMA_H_

#include <string>

#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/tests/test_case.h"

class TestUMA : public TestCase {
 public:
  explicit TestUMA(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestCount();
  std::string TestTime();
  std::string TestEnum();

  // Used by the tests that access the C API directly.
  const PPB_UMA_Private* uma_interface_;
};

#endif  // PPAPI_TESTS_TEST_UMA_H_
