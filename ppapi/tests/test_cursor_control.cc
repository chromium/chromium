// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_cursor_control.h"

#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(CursorControl);

TestCursorControl::TestCursorControl(TestingInstance* instance)
    : TestCase(instance),
      cursor_control_interface_(NULL) {
}

bool TestCursorControl::Init() {
  cursor_control_interface_ = static_cast<const PPB_CursorControl_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CURSOR_CONTROL_DEV_INTERFACE));
  return !!cursor_control_interface_;
}

void TestCursorControl::RunTests(const std::string& filter) {
  RUN_TEST(SetCursor, filter);
}

std::string TestCursorControl::TestSetCursor() {
  // Very simplistic test to make sure we can actually call the function and
  // it reports success. This is a nice integration test to make sure the
  // interface is hooked up. Obviously it's not easy in a plugin to test whether
  // the mouse cursor actually changed.
  ASSERT_TRUE(cursor_control_interface_->SetCursor(instance_->pp_instance(),
      PP_CURSORTYPE_WAIT, 0, NULL));

  PASS();
}
