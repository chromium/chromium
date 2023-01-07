// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_NACL_IRT_STACK_ALIGNMENT_H_
#define PPAPI_TESTS_TEST_NACL_IRT_STACK_ALIGNMENT_H_

#include <string>

#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_case.h"

class TestNaClIRTStackAlignment : public TestCase {
 public:
  explicit TestNaClIRTStackAlignment(TestingInstance* instance)
      : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestMisalignedCallVarAddRef();

  // Used by the tests that access the C API directly.
  const PPB_Var* var_interface_;
};

#endif  // PPAPI_TESTS_TEST_NACL_IRT_STACK_ALIGNMENT_H_
