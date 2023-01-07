// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_input_event.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(InputEvent);

namespace {

const uint32_t kSpaceChar = 0x20;
const char* kSpaceString = " ";
const char* kSpaceCode = "Space";

#define FINISHED_WAITING_MESSAGE "TEST_INPUT_EVENT_FINISHED_WAITING"

pp::Point GetCenter(const pp::Rect& rect) {
  return pp::Point(
      rect.x() + rect.width() / 2,
      rect.y() + rect.height() / 2);
}

}  // namespace

void TestInputEvent::RunTests(const std::string& filter) {
  RUN_TEST(Events, filter);

  // The AcceptTouchEvent_N tests should not be run when the filter is empty;
  // they can only be run one at a time.
  // TODO(dmichael): Figure out a way to make these run in the same test fixture
  //                 instance.
  if (!ShouldRunAllTests(filter)) {
    RUN_TEST(AcceptTouchEvent_1, filter);
    RUN_TEST(AcceptTouchEvent_2, filter);
    RUN_TEST(AcceptTouchEvent_3, filter);
    RUN_TEST(AcceptTouchEvent_4, filter);
  }
}

TestInputEvent::TestInputEvent(TestingInstance* instance)
    : TestCase(instance),
      input_event_interface_(NULL),
      mouse_input_event_interface_(NULL),
      wheel_input_event_interface_(NULL),
      keyboard_input_event_interface_(NULL),
      touch_input_event_interface_(NULL),
      nested_event_(instance->pp_instance()),
      view_rect_(),
      expected_input_event_(0),
      received_expected_event_(false),
      received_finish_message_(false) {
}

TestInputEvent::~TestInputEvent() {
  // Remove the special listener that only responds to a
  // FINISHED_WAITING_MESSAGE string. See Init for where it gets added.
  std::string js_code;
  js_code += "var plugin = document.getElementById('plugin');"
             "plugin.removeEventListener('message',"
             "                           plugin.wait_for_messages_handler);"
             "delete plugin.wait_for_messages_handler;";
  instance_->EvalScript(js_code);
}

bool TestInputEvent::Init() {
  input_event_interface_ = static_cast<const PPB_InputEvent*>(
      pp::Module::Get()->GetBrowserInterface(PPB_INPUT_EVENT_INTERFACE));
  mouse_input_event_interface_ = static_cast<const PPB_MouseInputEvent*>(
      pp::Module::Get()->GetBrowserInterface(
          PPB_MOUSE_INPUT_EVENT_INTERFACE));
  wheel_input_event_interface_ = static_cast<const PPB_WheelInputEvent*>(
      pp::Module::Get()->GetBrowserInterface(
          PPB_WHEEL_INPUT_EVENT_INTERFACE));
  keyboard_input_event_interface_ = static_cast<const PPB_KeyboardInputEvent*>(
      pp::Module::Get()->GetBrowserInterface(
          PPB_KEYBOARD_INPUT_EVENT_INTERFACE));
  touch_input_event_interface_ = static_cast<const PPB_TouchInputEvent*>(
      pp::Module::Get()->GetBrowserInterface(
          PPB_TOUCH_INPUT_EVENT_INTERFACE));

  bool success =
      input_event_interface_ &&
      mouse_input_event_interface_ &&
      wheel_input_event_interface_ &&
      keyboard_input_event_interface_ &&
      touch_input_event_interface_ &&
      CheckTestingInterface();

  // Set up a listener for our message that signals that all input events have
  // been received.
  std::string js_code;
  // Note the following code is dependent on some features of test_case.html.
  // E.g., it is assumed that the DOM element where the plugin is embedded has
  // an id of 'plugin', and there is a function 'IsTestingMessage' that allows
  // us to ignore the messages that are intended for use by the testing
  // framework itself.
  js_code += "var plugin = document.getElementById('plugin');"
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

pp::InputEvent TestInputEvent::CreateMouseEvent(
    PP_InputEvent_Type type,
    PP_InputEvent_MouseButton buttons) {
  return pp::MouseInputEvent(
      instance_,
      type,
      100,  // time_stamp
      0,  // modifiers
      buttons,
      GetCenter(view_rect_),
      1,  // click count
      pp::Point());  // movement
}

pp::InputEvent TestInputEvent::CreateWheelEvent() {
  return pp::WheelInputEvent(
      instance_,
      100,  // time_stamp
      0,  // modifiers
      pp::FloatPoint(1, 2),
      pp::FloatPoint(3, 4),
      PP_TRUE);  // scroll_by_page
}

pp::InputEvent TestInputEvent::CreateKeyEvent(PP_InputEvent_Type type,
                                              uint32_t key_code,
                                              const std::string& code) {
  return pp::KeyboardInputEvent(
      instance_,
      type,
      100,  // time_stamp
      0,  // modifiers
      key_code,
      pp::Var(),
      pp::Var(code));
}

pp::InputEvent TestInputEvent::CreateCharEvent(const std::string& text) {
  return pp::KeyboardInputEvent(
      instance_,
      PP_INPUTEVENT_TYPE_CHAR,
      100,  // time_stamp
      0,  // modifiers
      0,  // keycode
      pp::Var(text),
      pp::Var());
}

pp::InputEvent TestInputEvent::CreateTouchEvent(PP_InputEvent_Type type,
                                                const pp::FloatPoint& point) {
  PP_TouchPoint touch_point = PP_MakeTouchPoint();
  touch_point.position = point;

  pp::TouchInputEvent touch_event(instance_, type, 100, 0);
  touch_event.AddTouchPoint(PP_TOUCHLIST_TYPE_TOUCHES, touch_point);
  touch_event.AddTouchPoint(PP_TOUCHLIST_TYPE_CHANGEDTOUCHES, touch_point);
  touch_event.AddTouchPoint(PP_TOUCHLIST_TYPE_TARGETTOUCHES, touch_point);

  return touch_event;
}

void TestInputEvent::PostMessageBarrier() {
  received_finish_message_ = false;
  instance_->PostMessage(pp::Var(FINISHED_WAITING_MESSAGE));
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  nested_event_.Wait();
}

// Simulates the input event and calls PostMessage to let us know when
// we have received all resulting events from the browser.
bool TestInputEvent::SimulateInputEvent(
    const pp::InputEvent& input_event) {
  expected_input_event_ = pp::InputEvent(input_event.pp_resource());
  received_expected_event_ = false;
  testing_interface_->SimulateInputEvent(instance_->pp_instance(),
                                         input_event.pp_resource());
  PostMessageBarrier();
  return received_expected_event_;
}

bool TestInputEvent::AreEquivalentEvents(PP_Resource received,
                                         PP_Resource expected) {
  if (!input_event_interface_->IsInputEvent(received) ||
      !input_event_interface_->IsInputEvent(expected)) {
    return false;
  }

  // Test common fields, except modifiers and time stamp, which may be changed
  // by the browser.
  int32_t received_type = input_event_interface_->GetType(received);
  int32_t expected_type = input_event_interface_->GetType(expected);
  if (received_type != expected_type) {
    // Allow key down events to match "raw" key down events.
    if (expected_type != PP_INPUTEVENT_TYPE_KEYDOWN &&
        received_type != PP_INPUTEVENT_TYPE_RAWKEYDOWN) {
      return false;
    }
  }

  // Test event type-specific fields.
  switch (input_event_interface_->GetType(received)) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      // Check mouse fields, except position and movement, which may be
      // modified by the renderer.
      return
          mouse_input_event_interface_->GetButton(received) ==
          mouse_input_event_interface_->GetButton(expected) &&
          mouse_input_event_interface_->GetClickCount(received) ==
          mouse_input_event_interface_->GetClickCount(expected);

    case PP_INPUTEVENT_TYPE_WHEEL:
      return
          pp::FloatPoint(wheel_input_event_interface_->GetDelta(received)) ==
          pp::FloatPoint(wheel_input_event_interface_->GetDelta(expected)) &&
          pp::FloatPoint(wheel_input_event_interface_->GetTicks(received)) ==
          pp::FloatPoint(wheel_input_event_interface_->GetTicks(expected)) &&
          wheel_input_event_interface_->GetScrollByPage(received) ==
          wheel_input_event_interface_->GetScrollByPage(expected);

    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
      return
          keyboard_input_event_interface_->GetKeyCode(received) ==
          keyboard_input_event_interface_->GetKeyCode(expected);

    case PP_INPUTEVENT_TYPE_CHAR:
      return
          keyboard_input_event_interface_->GetKeyCode(received) ==
          keyboard_input_event_interface_->GetKeyCode(expected) &&
          pp::Var(pp::PASS_REF,
              keyboard_input_event_interface_->GetCharacterText(received)) ==
          pp::Var(pp::PASS_REF,
              keyboard_input_event_interface_->GetCharacterText(expected));

    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL: {
      if (!touch_input_event_interface_->IsTouchInputEvent(received) ||
          !touch_input_event_interface_->IsTouchInputEvent(expected))
        return false;

      uint32_t touch_count = touch_input_event_interface_->GetTouchCount(
          received, PP_TOUCHLIST_TYPE_TOUCHES);
      if (touch_count <= 0 ||
          touch_count != touch_input_event_interface_->GetTouchCount(expected,
              PP_TOUCHLIST_TYPE_TOUCHES))
        return false;

      for (uint32_t i = 0; i < touch_count; ++i) {
        PP_TouchPoint expected_point = touch_input_event_interface_->
            GetTouchByIndex(expected, PP_TOUCHLIST_TYPE_TOUCHES, i);
        PP_TouchPoint received_point = touch_input_event_interface_->
            GetTouchByIndex(received, PP_TOUCHLIST_TYPE_TOUCHES, i);

        if (expected_point.id != received_point.id ||
            expected_point.radius != received_point.radius ||
            expected_point.rotation_angle != received_point.rotation_angle ||
            expected_point.pressure != received_point.pressure)
          return false;

        if (expected_point.position.x != received_point.position.x ||
            expected_point.position.y != received_point.position.y)
          return false;
      }
      return true;
    }

    default:
      break;
  }

  return false;
}

bool TestInputEvent::HandleInputEvent(const pp::InputEvent& input_event) {
  // Some events may cause extra events to be generated, so look for the
  // first one that matches.
  if (!received_expected_event_) {
    received_expected_event_ = AreEquivalentEvents(
        input_event.pp_resource(),
        expected_input_event_.pp_resource());
  }
  // Handle all input events.
  return true;
}

void TestInputEvent::HandleMessage(const pp::Var& message_data) {
  if (message_data.is_string() &&
      (message_data.AsString() == FINISHED_WAITING_MESSAGE)) {
    testing_interface_->QuitMessageLoop(instance_->pp_instance());
    received_finish_message_ = true;
    nested_event_.Signal();
  }
}

void TestInputEvent::DidChangeView(const pp::View& view) {
  view_rect_ = view.GetRect();
}

std::string TestInputEvent::TestEvents() {
  // Request all input event classes.
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_MOUSE |
                                             PP_INPUTEVENT_CLASS_WHEEL |
                                             PP_INPUTEVENT_CLASS_KEYBOARD |
                                             PP_INPUTEVENT_CLASS_TOUCH);
  PostMessageBarrier();

  // Send the events and check that we received them.
  ASSERT_TRUE(
      SimulateInputEvent(CreateMouseEvent(PP_INPUTEVENT_TYPE_MOUSEDOWN,
                                          PP_INPUTEVENT_MOUSEBUTTON_LEFT)));
  ASSERT_TRUE(
      SimulateInputEvent(CreateWheelEvent()));
  ASSERT_TRUE(
      SimulateInputEvent(CreateKeyEvent(PP_INPUTEVENT_TYPE_KEYDOWN,
                                        kSpaceChar, kSpaceCode)));
  ASSERT_TRUE(
      SimulateInputEvent(CreateCharEvent(kSpaceString)));
  ASSERT_TRUE(SimulateInputEvent(CreateTouchEvent(PP_INPUTEVENT_TYPE_TOUCHSTART,
                                                  pp::FloatPoint(12, 23))));
  // Request only mouse events.
  input_event_interface_->ClearInputEventRequest(instance_->pp_instance(),
                                                 PP_INPUTEVENT_CLASS_WHEEL |
                                                 PP_INPUTEVENT_CLASS_KEYBOARD);
  PostMessageBarrier();

  // Check that we only receive mouse events.
  ASSERT_TRUE(
      SimulateInputEvent(CreateMouseEvent(PP_INPUTEVENT_TYPE_MOUSEDOWN,
                                          PP_INPUTEVENT_MOUSEBUTTON_LEFT)));
  ASSERT_FALSE(
      SimulateInputEvent(CreateWheelEvent()));
  ASSERT_FALSE(
      SimulateInputEvent(CreateKeyEvent(PP_INPUTEVENT_TYPE_KEYDOWN,
                                        kSpaceChar, kSpaceCode)));
  ASSERT_FALSE(
      SimulateInputEvent(CreateCharEvent(kSpaceString)));

  PASS();
}

std::string TestInputEvent::TestAcceptTouchEvent_1() {
  // The browser normally sends touch-events to the renderer only if the page
  // has touch-event handlers. Since test-case.html does not have any
  // touch-event handler, it would normally not receive any touch events from
  // the browser. However, if a plugin in the page does accept touch events,
  // then the browser should start sending touch-events to the page. In this
  // test, the plugin simply registers for touch-events. The real test is to
  // verify that the browser knows to send touch-events to the renderer.
  // If the plugin is removed from the page, then there are no more touch-event
  // handlers in the page, and browser stops sending touch-events. So to make
  // it possible to test this properly, the plugin is not removed from the page
  // at the end of the test.
  instance_->set_remove_plugin(false);
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_MOUSE |
                                             PP_INPUTEVENT_CLASS_WHEEL |
                                             PP_INPUTEVENT_CLASS_KEYBOARD |
                                             PP_INPUTEVENT_CLASS_TOUCH);
  PASS();
}

std::string TestInputEvent::TestAcceptTouchEvent_2() {
  // See comment in TestAcceptTouchEvent_1.
  instance_->set_remove_plugin(false);
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_MOUSE |
                                             PP_INPUTEVENT_CLASS_WHEEL |
                                             PP_INPUTEVENT_CLASS_KEYBOARD |
                                             PP_INPUTEVENT_CLASS_TOUCH);
  input_event_interface_->ClearInputEventRequest(instance_->pp_instance(),
                                                 PP_INPUTEVENT_CLASS_TOUCH);
  PASS();
}

std::string TestInputEvent::TestAcceptTouchEvent_3() {
  // See comment in TestAcceptTouchEvent_1.
  instance_->set_remove_plugin(false);
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_MOUSE |
                                             PP_INPUTEVENT_CLASS_WHEEL |
                                             PP_INPUTEVENT_CLASS_KEYBOARD);
  input_event_interface_->RequestFilteringInputEvents(instance_->pp_instance(),
      PP_INPUTEVENT_CLASS_TOUCH);
  PASS();
}

std::string TestInputEvent::TestAcceptTouchEvent_4() {
  // See comment in TestAcceptTouchEvent_1.
  instance_->set_remove_plugin(false);
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_MOUSE |
                                             PP_INPUTEVENT_CLASS_WHEEL |
                                             PP_INPUTEVENT_CLASS_KEYBOARD);
  input_event_interface_->RequestInputEvents(instance_->pp_instance(),
                                             PP_INPUTEVENT_CLASS_TOUCH);
  PASS();
}
