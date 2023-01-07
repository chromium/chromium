// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_ime_input_event.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(ImeInputEvent);

namespace {

// Japanese Kanji letters
const char* kCompositionChar[] = {
    "\xE6\x96\x87",  // An example character of normal unicode.
    "\xF0\xA0\xAE\x9F", // An example character of surrogate pair.
    "\xF0\x9F\x98\x81"  // An example character of surrogate pair(emoji).
};

const char kCompositionText[] = "\xE6\x96\x87\xF0\xA0\xAE\x9F\xF0\x9F\x98\x81";

#define FINISHED_WAITING_MESSAGE "TEST_IME_INPUT_EVENT_FINISHED_WAITING"

}  // namespace

TestImeInputEvent::TestImeInputEvent(TestingInstance* instance)
    : TestCase(instance),
      input_event_interface_(NULL),
      keyboard_input_event_interface_(NULL),
      ime_input_event_interface_(NULL),
      received_unexpected_event_(true),
      received_finish_message_(false) {
}

TestImeInputEvent::~TestImeInputEvent() {
  // Remove the special listener that only responds to a
  // FINISHED_WAITING_MESSAGE string. See Init for where it gets added.
  std::string js_code;
  js_code = "var plugin = document.getElementById('plugin');"
            "plugin.removeEventListener('message',"
            "                           plugin.wait_for_messages_handler);"
            "delete plugin.wait_for_messages_handler;";
  instance_->EvalScript(js_code);
}

void TestImeInputEvent::RunTests(const std::string& filter) {
  RUN_TEST(ImeCommit, filter);
  RUN_TEST(ImeCancel, filter);
  RUN_TEST(ImeUnawareCommit, filter);
  RUN_TEST(ImeUnawareCancel, filter);
}

bool TestImeInputEvent::Init() {
  input_event_interface_ = static_cast<const PPB_InputEvent*>(
      pp::Module::Get()->GetBrowserInterface(PPB_INPUT_EVENT_INTERFACE));
  keyboard_input_event_interface_ =
      static_cast<const PPB_KeyboardInputEvent*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_KEYBOARD_INPUT_EVENT_INTERFACE));
  ime_input_event_interface_ = static_cast<const PPB_IMEInputEvent*>(
      pp::Module::Get()->GetBrowserInterface(
          PPB_IME_INPUT_EVENT_INTERFACE));

  bool success =
      input_event_interface_ &&
      keyboard_input_event_interface_ &&
      ime_input_event_interface_ &&
      CheckTestingInterface();

  // Set up a listener for our message that signals that all input events have
  // been received.
  // Note the following code is dependent on some features of test_case.html.
  // E.g., it is assumed that the DOM element where the plugin is embedded has
  // an id of 'plugin', and there is a function 'IsTestingMessage' that allows
  // us to ignore the messages that are intended for use by the testing
  // framework itself.
  std::string js_code =
      "var plugin = document.getElementById('plugin');"
      "var wait_for_messages_handler = function(message_event) {"
      "  if (!IsTestingMessage(message_event.data) &&"
      "      message_event.data === '" FINISHED_WAITING_MESSAGE "') {"
      "    plugin.postMessage('" FINISHED_WAITING_MESSAGE "');"
      "  }"
      "};"
      "plugin.addEventListener('message', wait_for_messages_handler);"
      // Stash it on the plugin so we can remove it in the destructor.
      "plugin.wait_for_messages_handler = wait_for_messages_handler;";
  instance_->EvalScript(js_code);

  return success;
}

bool TestImeInputEvent::HandleInputEvent(const pp::InputEvent& input_event) {
  // Check whether the IME related events comes in the expected order.
  switch (input_event.GetType()) {
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
    case PP_INPUTEVENT_TYPE_CHAR:
      if (expected_events_.empty()) {
        received_unexpected_event_ = true;
      } else {
        received_unexpected_event_ =
          !AreEquivalentEvents(input_event.pp_resource(),
                               expected_events_.front().pp_resource());
        expected_events_.erase(expected_events_.begin());
      }
      break;

    default:
      // Don't care for any other input event types for this test.
      break;
  }

  // Handle all input events.
  return true;
}

void TestImeInputEvent::HandleMessage(const pp::Var& message_data) {
  if (message_data.is_string() &&
      (message_data.AsString() == FINISHED_WAITING_MESSAGE)) {
    testing_interface_->QuitMessageLoop(instance_->pp_instance());
    received_finish_message_ = true;
  }
}

void TestImeInputEvent::DidChangeView(const pp::View& view) {
  view_rect_ = view.GetRect();
}

pp::InputEvent TestImeInputEvent::CreateImeCompositionStartEvent() {
  return pp::IMEInputEvent(
      instance_,
      PP_INPUTEVENT_TYPE_IME_COMPOSITION_START,
      100, // time_stamp
      pp::Var(""),
      std::vector<uint32_t>(),
      -1, // target_segment
      std::make_pair(0U, 0U) // selection
  );
}

pp::InputEvent TestImeInputEvent::CreateImeCompositionUpdateEvent(
    const std::string& text,
    const std::vector<uint32_t>& segments,
    int32_t target_segment,
    const std::pair<uint32_t, uint32_t>& selection) {
  return pp::IMEInputEvent(
      instance_,
      PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE,
      100, // time_stamp
      text,
      segments,
      target_segment,
      selection
  );
}

pp::InputEvent TestImeInputEvent::CreateImeCompositionEndEvent(
    const std::string& text) {
  return pp::IMEInputEvent(
      instance_,
      PP_INPUTEVENT_TYPE_IME_COMPOSITION_END,
      100, // time_stamp
      pp::Var(text),
      std::vector<uint32_t>(),
      -1, // target_segment
      std::make_pair(0U, 0U) // selection
  );
}

pp::InputEvent TestImeInputEvent::CreateImeTextEvent(const std::string& text) {
  return pp::IMEInputEvent(
      instance_,
      PP_INPUTEVENT_TYPE_IME_TEXT,
      100, // time_stamp
      pp::Var(text),
      std::vector<uint32_t>(),
      -1, // target_segment
      std::make_pair(0U, 0U) // selection
  );
}

pp::InputEvent TestImeInputEvent::CreateCharEvent(const std::string& text) {
  return pp::KeyboardInputEvent(
      instance_,
      PP_INPUTEVENT_TYPE_CHAR,
      100,  // time_stamp
      0,  // modifiers
      0,  // keycode
      pp::Var(text),
      pp::Var());
}

void TestImeInputEvent::GetFocusBySimulatingMouseClick() {
  // For receiving IME events, the plugin DOM node needs to be focused.
  // The following code is for achieving that by simulating a mouse click event.
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_MOUSE);
  SimulateInputEvent(pp::MouseInputEvent(
      instance_,
      PP_INPUTEVENT_TYPE_MOUSEDOWN,
      100,  // time_stamp
      0,  // modifiers
      PP_INPUTEVENT_MOUSEBUTTON_LEFT,
      pp::Point(
          view_rect_.x() + view_rect_.width() / 2,
          view_rect_.y() + view_rect_.height() / 2),
      1,  // click count
      pp::Point()));  // movement
}

// Simulates the input event and calls PostMessage to let us know when
// we have received all resulting events from the browser.
bool TestImeInputEvent::SimulateInputEvent(const pp::InputEvent& input_event) {
  received_unexpected_event_ = false;
  received_finish_message_ = false;
  testing_interface_->SimulateInputEvent(instance_->pp_instance(),
                                         input_event.pp_resource());
  instance_->PostMessage(pp::Var(FINISHED_WAITING_MESSAGE));
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  return received_finish_message_ && !received_unexpected_event_;
}

bool TestImeInputEvent::AreEquivalentEvents(PP_Resource received,
                                            PP_Resource expected) {
  if (!input_event_interface_->IsInputEvent(received) ||
      !input_event_interface_->IsInputEvent(expected)) {
    return false;
  }

  // Test common fields, except modifiers and time stamp, which may be changed
  // by the browser.
  int32_t received_type = input_event_interface_->GetType(received);
  int32_t expected_type = input_event_interface_->GetType(expected);
  if (received_type != expected_type)
    return false;

  // Test event type-specific fields.
  switch (received_type) {
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
      // COMPOSITION_START does not convey further information.
      break;

    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      // For COMPOSITION_END and TEXT, GetText() has meaning.
      return pp::Var(pp::PASS_REF,
                     ime_input_event_interface_->GetText(received)) ==
             pp::Var(pp::PASS_REF,
                     ime_input_event_interface_->GetText(expected));

    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
      // For COMPOSITION_UPDATE, all fields must be checked.
      {
        uint32_t received_segment_number =
            ime_input_event_interface_->GetSegmentNumber(received);
        uint32_t expected_segment_number =
            ime_input_event_interface_->GetSegmentNumber(expected);
        if (received_segment_number != expected_segment_number)
          return false;

        // The "<=" is not a bug. i-th segment is represented as the pair of
        // i-th and (i+1)-th offsets in Pepper IME API.
        for (uint32_t i = 0; i <= received_segment_number; ++i) {
          if (ime_input_event_interface_->GetSegmentOffset(received, i) !=
              ime_input_event_interface_->GetSegmentOffset(expected, i))
            return false;
        }

        uint32_t received_selection_start = 0;
        uint32_t received_selection_end = 0;
        uint32_t expected_selection_start = 0;
        uint32_t expected_selection_end = 0;
        ime_input_event_interface_->GetSelection(
            received, &received_selection_start, &received_selection_end);
        ime_input_event_interface_->GetSelection(
            expected, &expected_selection_start, &expected_selection_end);
        if (received_selection_start != expected_selection_start ||
            received_selection_end != expected_selection_end) {
          return true;
        }

        return pp::Var(pp::PASS_REF,
                       ime_input_event_interface_->GetText(received)) ==
               pp::Var(pp::PASS_REF,
                       ime_input_event_interface_->GetText(expected)) &&
               ime_input_event_interface_->GetTargetSegment(received) ==
               ime_input_event_interface_->GetTargetSegment(expected);
      }

    case PP_INPUTEVENT_TYPE_CHAR:
      return
          keyboard_input_event_interface_->GetKeyCode(received) ==
          keyboard_input_event_interface_->GetKeyCode(expected) &&
          pp::Var(pp::PASS_REF,
              keyboard_input_event_interface_->GetCharacterText(received)) ==
          pp::Var(pp::PASS_REF,
              keyboard_input_event_interface_->GetCharacterText(expected));

    default:
      break;
  }
  return true;
}

std::string TestImeInputEvent::TestImeCommit() {
  GetFocusBySimulatingMouseClick();

  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_KEYBOARD |
                                             PP_INPUTEVENT_CLASS_IME);

  std::vector<uint32_t> segments;
  segments.push_back(0U);
  segments.push_back(3U);
  segments.push_back(7U);
  segments.push_back(11U);
  pp::InputEvent update_event = CreateImeCompositionUpdateEvent(
      kCompositionText, segments, 1, std::make_pair(3U, 7U));

  expected_events_.clear();
  expected_events_.push_back(CreateImeCompositionStartEvent());
  expected_events_.push_back(update_event);
  expected_events_.push_back(CreateImeCompositionEndEvent(kCompositionText));
  expected_events_.push_back(CreateImeTextEvent(kCompositionText));

  // Simulate the case when IME successfully committed some text.
  ASSERT_TRUE(SimulateInputEvent(update_event));
  ASSERT_TRUE(SimulateInputEvent(CreateImeTextEvent(kCompositionText)));

  ASSERT_TRUE(expected_events_.empty());
  PASS();
}

std::string TestImeInputEvent::TestImeCancel() {
  GetFocusBySimulatingMouseClick();

  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_KEYBOARD |
                                             PP_INPUTEVENT_CLASS_IME);

  std::vector<uint32_t> segments;
  segments.push_back(0U);
  segments.push_back(3U);
  segments.push_back(7U);
  segments.push_back(11U);
  pp::InputEvent update_event = CreateImeCompositionUpdateEvent(
      kCompositionText, segments, 1, std::make_pair(3U, 7U));

  expected_events_.clear();
  expected_events_.push_back(CreateImeCompositionStartEvent());
  expected_events_.push_back(update_event);
  expected_events_.push_back(CreateImeCompositionEndEvent(std::string()));

  // Simulate the case when IME canceled composition.
  ASSERT_TRUE(SimulateInputEvent(update_event));
  ASSERT_TRUE(SimulateInputEvent(CreateImeCompositionEndEvent(std::string())));

  ASSERT_TRUE(expected_events_.empty());
  PASS();
}

std::string TestImeInputEvent::TestImeUnawareCommit() {
  GetFocusBySimulatingMouseClick();

  input_event_interface_->ClearInputEventRequest(instance_->pp_instance(),
                                                 PP_INPUTEVENT_CLASS_IME);
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_KEYBOARD);

  std::vector<uint32_t> segments;
  segments.push_back(0U);
  segments.push_back(3U);
  segments.push_back(7U);
  segments.push_back(11U);
  pp::InputEvent update_event = CreateImeCompositionUpdateEvent(
      kCompositionText, segments, 1, std::make_pair(3U, 7U));

  expected_events_.clear();
  expected_events_.push_back(CreateCharEvent(kCompositionChar[0]));
  expected_events_.push_back(CreateCharEvent(kCompositionChar[1]));
  expected_events_.push_back(CreateCharEvent(kCompositionChar[2]));

  // Test for IME-unaware plugins. Commit event is translated to char events.
  ASSERT_TRUE(SimulateInputEvent(update_event));
  ASSERT_TRUE(SimulateInputEvent(CreateImeTextEvent(kCompositionText)));

  ASSERT_TRUE(expected_events_.empty());
  PASS();
}


std::string TestImeInputEvent::TestImeUnawareCancel() {
  GetFocusBySimulatingMouseClick();

  input_event_interface_->ClearInputEventRequest(instance_->pp_instance(),
                                                 PP_INPUTEVENT_CLASS_IME);
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_KEYBOARD);

  std::vector<uint32_t> segments;
  segments.push_back(0U);
  segments.push_back(3U);
  segments.push_back(7U);
  segments.push_back(11U);
  pp::InputEvent update_event = CreateImeCompositionUpdateEvent(
      kCompositionText, segments, 1, std::make_pair(3U, 7U));

  expected_events_.clear();

  // Test for IME-unaware plugins. Cancel won't issue any events.
  ASSERT_TRUE(SimulateInputEvent(update_event));
  ASSERT_TRUE(SimulateInputEvent(CreateImeCompositionEndEvent(std::string())));

  ASSERT_TRUE(expected_events_.empty());
  PASS();
}
