// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_FILE_SYSTEM_H_
#define PPAPI_TESTS_TEST_FILE_SYSTEM_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestFileSystem : public TestCase {
 public:
  explicit TestFileSystem(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestOpen();
  std::string TestMultipleOpens();
  std::string TestResourceConversion();
};

#endif  // PPAPI_TESTS_TEST_FILE_SYSTEM_H_

