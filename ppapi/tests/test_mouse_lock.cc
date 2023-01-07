// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_mouse_lock.h"

#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/view.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(MouseLock);

TestMouseLock::TestMouseLock(TestingInstance* instance)
    : TestCase(instance),
      MouseLock(instance),
      nested_event_(instance->pp_instance()) {
}

TestMouseLock::~TestMouseLock() {
}

bool TestMouseLock::Init() {
  return CheckTestingInterface();
}

void TestMouseLock::RunTests(const std::string& filter) {
  RUN_TEST(SucceedWhenAllowed, filter);
}

void TestMouseLock::DidChangeView(const pp::View& view) {
  position_ = view.GetRect();
}

void TestMouseLock::MouseLockLost() {
  nested_event_.Signal();
}

std::string TestMouseLock::TestSucceedWhenAllowed() {
  // Content settings are configured to allow mouse lock for any site.
  // Please see chrome/test/ppapi/ppapi_interactive_browsertest.cc.
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  SimulateUserGesture();
  callback.WaitForResult(LockMouse(callback.GetCallback()));
  ASSERT_EQ(PP_OK, callback.result());

  UnlockMouse();
  // Wait for the MouseLockLost() call.
  nested_event_.Wait();

  PASS();
}

void TestMouseLock::SimulateUserGesture() {
  pp::Point mouse_movement;
  pp::MouseInputEvent input_event(
      instance_,
      PP_INPUTEVENT_TYPE_MOUSEDOWN,
      0,  // time_stamp
      0,  // modifiers
      PP_INPUTEVENT_MOUSEBUTTON_LEFT,
      position_.CenterPoint(),
      1,  // click_count
      mouse_movement);

  testing_interface_->SimulateInputEvent(instance_->pp_instance(),
                                         input_event.pp_resource());
}
