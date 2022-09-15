// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_VAR_RESOURCE_H_
#define PPAPI_TESTS_TEST_VAR_RESOURCE_H_

#include <string>

#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/tests/test_case.h"

class TestVarResource : public TestCase {
 public:
  explicit TestVarResource(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestBasicResource();
  std::string TestInvalidAndEmpty();
  std::string TestWrongType();

  // Used by the tests that access the C APIs directly.
  const PPB_Core* core_interface_;
  const PPB_FileSystem* file_system_interface_;
  const PPB_Var* var_interface_;
};

#endif  // PPAPI_TESTS_TEST_VAR_RESOURCE_H_
