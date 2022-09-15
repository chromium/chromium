// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_MEMORY_H_
#define PPAPI_TESTS_TEST_MEMORY_H_

#include <string>

#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/tests/test_case.h"

class TestMemory : public TestCase {
 public:
  explicit TestMemory(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestMemAlloc();
  std::string TestNullMemFree();

  // Used by the tests that access the C API directly.
  const PPB_Memory_Dev* memory_dev_interface_;
};

#endif  // PPAPI_TESTS_TEST_MEMORY_H_
