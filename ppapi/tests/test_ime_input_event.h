// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_IME_INPUT_EVENT_H_
#define PPAPI_TESTS_TEST_IME_INPUT_EVENT_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/tests/test_case.h"

class TestImeInputEvent : public TestCase {
 public:
  explicit TestImeInputEvent(TestingInstance* instance);
  ~TestImeInputEvent();

  // TestCase implementation.
  virtual void RunTests(const std::string& test_filter);
  virtual bool Init();
  virtual bool HandleInputEvent(const pp::InputEvent& input_event);
  virtual void HandleMessage(const pp::Var& message_data);
  virtual void DidChangeView(const pp::View& view);

 private:
  pp::InputEvent CreateImeCompositionStartEvent();
  pp::InputEvent CreateImeCompositionUpdateEvent(
      const std::string& text,
      const std::vector<uint32_t>& segments,
      int32_t target_segment,
      const std::pair<uint32_t, uint32_t>& selection);
  pp::InputEvent CreateImeCompositionEndEvent(const std::string& text);
  pp::InputEvent CreateImeTextEvent(const std::string& text);
  pp::InputEvent CreateCharEvent(const std::string& text);

  void GetFocusBySimulatingMouseClick();
  bool SimulateInputEvent(const pp::InputEvent& input_event);
  bool AreEquivalentEvents(PP_Resource first, PP_Resource second);

  // The test cases.
  std::string TestImeCommit();
  std::string TestImeCancel();
  std::string TestImeUnawareCommit();
  std::string TestImeUnawareCancel();

  const PPB_InputEvent* input_event_interface_;
  const PPB_KeyboardInputEvent* keyboard_input_event_interface_;
  const PPB_IMEInputEvent* ime_input_event_interface_;

  pp::Rect view_rect_;
  bool received_unexpected_event_;
  bool received_finish_message_;
  std::vector<pp::InputEvent> expected_events_;
};

#endif  // PPAPI_TESTS_TEST_IME_INPUT_EVENT_H_
