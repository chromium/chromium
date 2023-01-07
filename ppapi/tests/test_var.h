// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_VAR_H_
#define PPAPI_TESTS_TEST_VAR_H_

#include <string>

#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_case.h"

class TestVar : public TestCase {
 public:
  explicit TestVar(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestBasicString();
  std::string TestInvalidAndEmpty();
  std::string TestInvalidUtf8();
  std::string TestNullInputInUtf8Conversion();
  std::string TestValidUtf8();
  std::string TestUtf8WithEmbeddedNulls();
  std::string TestVarToUtf8ForWrongType();
  std::string TestHasPropertyAndMethod();

  // Used by the tests that access the C API directly.
  const PPB_Var* var_interface_;
};

#endif  // PPAPI_TESTS_TEST_VAR_H_
