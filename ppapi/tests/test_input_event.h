// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_INPUT_EVENT_H_
#define PPAPI_TESTS_TEST_INPUT_EVENT_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"

class TestInputEvent : public TestCase {
 public:
  explicit TestInputEvent(TestingInstance* instance);
  ~TestInputEvent();

  virtual bool HandleInputEvent(const pp::InputEvent& input_event);
  virtual void HandleMessage(const pp::Var& message_data);
  virtual void DidChangeView(const pp::View& view);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& test_filter);

 private:
  pp::InputEvent CreateMouseEvent(PP_InputEvent_Type type,
                                  PP_InputEvent_MouseButton buttons);
  pp::InputEvent CreateWheelEvent();
  pp::InputEvent CreateKeyEvent(PP_InputEvent_Type type,
                                uint32_t key_code, const std::string& code);
  pp::InputEvent CreateCharEvent(const std::string& text);
  pp::InputEvent CreateTouchEvent(PP_InputEvent_Type type,
                                  const pp::FloatPoint& location);

  void PostMessageBarrier();
  bool SimulateInputEvent(const pp::InputEvent& input_event);
  bool AreEquivalentEvents(PP_Resource first, PP_Resource second);

  std::string TestEvents();
  std::string TestAcceptTouchEvent_1();
  std::string TestAcceptTouchEvent_2();
  std::string TestAcceptTouchEvent_3();
  std::string TestAcceptTouchEvent_4();

  const PPB_InputEvent* input_event_interface_;
  const PPB_MouseInputEvent* mouse_input_event_interface_;
  const PPB_WheelInputEvent* wheel_input_event_interface_;
  const PPB_KeyboardInputEvent* keyboard_input_event_interface_;
  const PPB_TouchInputEvent* touch_input_event_interface_;

  NestedEvent nested_event_;

  pp::Rect view_rect_;
  pp::InputEvent expected_input_event_;
  bool received_expected_event_;
  bool received_finish_message_;
};

#endif  // PPAPI_TESTS_TEST_INPUT_EVENT_H_
