// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CONSOLE_H_
#define PPAPI_TESTS_TEST_CONSOLE_H_

#include <string>

#include "ppapi/c/ppb_console.h"
#include "ppapi/tests/test_case.h"

class TestConsole : public TestCase {
 public:
  explicit TestConsole(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestSmoke();

  const PPB_Console* console_interface_;
};

#endif  // PPAPI_TESTS_TEST_CONSOLE_H_
