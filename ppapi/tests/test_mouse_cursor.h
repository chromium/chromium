// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_MOUSE_CURSOR_H_
#define PPAPI_TESTS_TEST_MOUSE_CURSOR_H_

#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/tests/test_case.h"

class TestMouseCursor : public TestCase {
 public:
  explicit TestMouseCursor(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestType();
  std::string TestCustom();
  std::string TestPoint();

  const PPB_MouseCursor* mouse_cursor_interface_;
};

#endif  // PPAPI_TESTS_TEST_MOUSE_CURSOR_H_
