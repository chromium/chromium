// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CURSOR_CONTROL_H_
#define PPAPI_TESTS_TEST_CURSOR_CONTROL_H_

#include <string>

#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/tests/test_case.h"

class TestCursorControl : public TestCase {
 public:
  TestCursorControl(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestSetCursor();

  const PPB_CursorControl_Dev* cursor_control_interface_;
};

#endif  // PPAPI_TESTS_TEST_CURSOR_CONTROL_H_
