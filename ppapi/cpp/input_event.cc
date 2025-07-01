// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/input_event.h"

#include "ppapi/cpp/input_event_interface_name.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/touch_point.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_KeyboardInputEvent_1_2>() {
  return PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_2;
}

template <> const char* interface_name<PPB_KeyboardInputEvent_1_0>() {
  return PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_MouseInputEvent_1_1>() {
  return PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1;
}

template <> const char* interface_name<PPB_WheelInputEvent_1_0>() {
  return PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_TouchInputEvent_1_0>() {
  return PPB_TOUCH_INPUT_EVENT_INTERFACE_1_0;
}

template <>
const char* interface_name<PPB_TouchInputEvent_1_4>() {
  return PPB_TOUCH_INPUT_EVENT_INTERFACE_1_4;
}

template <> const char* interface_name<PPB_IMEInputEvent_1_0>() {
  return PPB_IME_INPUT_EVENT_INTERFACE_1_0;
}

}  // namespace

// InputEvent ------------------------------------------------------------------

InputEvent::InputEvent() : Resource() {
}

InputEvent::InputEvent(PP_Resource input_event_resource) : Resource() {
  // Type check the input event before setting it.
  if (!has_interface<PPB_InputEvent_1_0>())
    return;
  if (get_interface<PPB_InputEvent_1_0>()->IsInputEvent(input_event_resource)) {
    Module::Get()->core()->AddRefResource(input_event_resource);
    PassRefFromConstructor(input_event_resource);
  }
}

InputEvent::~InputEvent() {
}

PP_InputEvent_Type InputEvent::GetType() const {
  if (!has_interface<PPB_InputEvent_1_0>())
    return PP_INPUTEVENT_TYPE_UNDEFINED;
  return get_interface<PPB_InputEvent_1_0>()->GetType(pp_resource());
}

PP_TimeTicks InputEvent::GetTimeStamp() const {
  if (!has_interface<PPB_InputEvent_1_0>())
    return 0.0f;
  return get_interface<PPB_InputEvent_1_0>()->GetTimeStamp(pp_resource());
}

uint32_t InputEvent::GetModifiers() const {
  if (!has_interface<PPB_InputEvent_1_0>())
    return 0;
  return get_interface<PPB_InputEvent_1_0>()->GetModifiers(pp_resource());
}

// MouseInputEvent -------------------------------------------------------------

MouseInputEvent::MouseInputEvent() : InputEvent() {
}

MouseInputEvent::MouseInputEvent(const InputEvent& event) : InputEvent() {
  // Type check the input event before setting it.
  if (!has_interface<PPB_MouseInputEvent_1_1>())
    return;
  if (get_interface<PPB_MouseInputEvent_1_1>()->IsMouseInputEvent(
          event.pp_resource())) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

MouseInputEvent::MouseInputEvent(const InstanceHandle& instance,
                                 PP_InputEvent_Type type,
                                 PP_TimeTicks time_stamp,
                                 uint32_t modifiers,
                                 PP_InputEvent_MouseButton mouse_button,
                                 const Point& mouse_position,
                                 int32_t click_count,
                                 const Point& mouse_movement) {
  // Type check the input event before setting it.
  if (!has_interface<PPB_MouseInputEvent_1_1>())
    return;
  PassRefFromConstructor(get_interface<PPB_MouseInputEvent_1_1>()->Create(
      instance.pp_instance(), type, time_stamp, modifiers, mouse_button,
      &mouse_position.pp_point(), click_count, &mouse_movement.pp_point()));
}

PP_InputEvent_MouseButton MouseInputEvent::GetButton() const {
  if (!has_interface<PPB_MouseInputEvent_1_1>())
    return PP_INPUTEVENT_MOUSEBUTTON_NONE;
  return get_interface<PPB_MouseInputEvent_1_1>()->GetButton(pp_resource());
}

Point MouseInputEvent::GetPosition() const {
  if (!has_interface<PPB_MouseInputEvent_1_1>())
    return Point();
  return get_interface<PPB_MouseInputEvent_1_1>()->GetPosition(pp_resource());
}

int32_t MouseInputEvent::GetClickCount() const {
  if (!has_interface<PPB_MouseInputEvent_1_1>())
    return 0;
  return get_interface<PPB_MouseInputEvent_1_1>()->GetClickCount(pp_resource());
}

Point MouseInputEvent::GetMovement() const {
  if (!has_interface<PPB_MouseInputEvent_1_1>())
    return Point();
  return get_interface<PPB_MouseInputEvent_1_1>()->GetMovement(pp_resource());
}

// WheelInputEvent -------------------------------------------------------------

WheelInputEvent::WheelInputEvent() : InputEvent() {
}

WheelInputEvent::WheelInputEvent(const InputEvent& event) : InputEvent() {
  // Type check the input event before setting it.
  if (!has_interface<PPB_WheelInputEvent_1_0>())
    return;
  if (get_interface<PPB_WheelInputEvent_1_0>()->IsWheelInputEvent(
          event.pp_resource())) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

WheelInputEvent::WheelInputEvent(const InstanceHandle& instance,
                                 PP_TimeTicks time_stamp,
                                 uint32_t modifiers,
                                 const FloatPoint& wheel_delta,
                                 const FloatPoint& wheel_ticks,
                                 bool scroll_by_page) {
  // Type check the input event before setting it.
  if (!has_interface<PPB_WheelInputEvent_1_0>())
    return;
  PassRefFromConstructor(get_interface<PPB_WheelInputEvent_1_0>()->Create(
      instance.pp_instance(), time_stamp, modifiers,
      &wheel_delta.pp_float_point(), &wheel_ticks.pp_float_point(),
      PP_FromBool(scroll_by_page)));
}

FloatPoint WheelInputEvent::GetDelta() const {
  if (!has_interface<PPB_WheelInputEvent_1_0>())
    return FloatPoint();
  return get_interface<PPB_WheelInputEvent_1_0>()->GetDelta(pp_resource());
}

FloatPoint WheelInputEvent::GetTicks() const {
  if (!has_interface<PPB_WheelInputEvent_1_0>())
    return FloatPoint();
  return get_interface<PPB_WheelInputEvent_1_0>()->GetTicks(pp_resource());
}

bool WheelInputEvent::GetScrollByPage() const {
  if (!has_interface<PPB_WheelInputEvent_1_0>())
    return false;
  return PP_ToBool(
      get_interface<PPB_WheelInputEvent_1_0>()->GetScrollByPage(pp_resource()));
}

// KeyboardInputEvent ----------------------------------------------------------

KeyboardInputEvent::KeyboardInputEvent() : InputEvent() {
}

KeyboardInputEvent::KeyboardInputEvent(const InputEvent& event) : InputEvent() {
  PP_Bool is_keyboard_event = PP_FALSE;

  if (has_interface<PPB_KeyboardInputEvent_1_2>()) {
    is_keyboard_event =
        get_interface<PPB_KeyboardInputEvent_1_2>()->IsKeyboardInputEvent(
            event.pp_resource());
  } else if (has_interface<PPB_KeyboardInputEvent_1_0>()) {
    is_keyboard_event =
        get_interface<PPB_KeyboardInputEvent_1_0>()->IsKeyboardInputEvent(
            event.pp_resource());
  }

  if (PP_ToBool(is_keyboard_event)) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

KeyboardInputEvent::KeyboardInputEvent(const InstanceHandle& instance,
                                       PP_InputEvent_Type type,
                                       PP_TimeTicks time_stamp,
                                       uint32_t modifiers,
                                       uint32_t key_code,
                                       const Var& character_text) {
  if (has_interface<PPB_KeyboardInputEvent_1_2>()) {
    PassRefFromConstructor(get_interface<PPB_KeyboardInputEvent_1_2>()->Create(
        instance.pp_instance(), type, time_stamp, modifiers, key_code,
        character_text.pp_var(), Var().pp_var()));
  } else if (has_interface<PPB_KeyboardInputEvent_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_KeyboardInputEvent_1_0>()->Create(
        instance.pp_instance(), type, time_stamp, modifiers, key_code,
        character_text.pp_var()));
  }
}

KeyboardInputEvent::KeyboardInputEvent(const InstanceHandle& instance,
                                       PP_InputEvent_Type type,
                                       PP_TimeTicks time_stamp,
                                       uint32_t modifiers,
                                       uint32_t key_code,
                                       const Var& character_text,
                                       const Var& code) {
  if (has_interface<PPB_KeyboardInputEvent_1_2>()) {
    PassRefFromConstructor(get_interface<PPB_KeyboardInputEvent_1_2>()->Create(
        instance.pp_instance(), type, time_stamp, modifiers, key_code,
        character_text.pp_var(), code.pp_var()));
  } else if (has_interface<PPB_KeyboardInputEvent_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_KeyboardInputEvent_1_0>()->Create(
        instance.pp_instance(), type, time_stamp, modifiers, key_code,
        character_text.pp_var()));
  }
}

uint32_t KeyboardInputEvent::GetKeyCode() const {
  if (has_interface<PPB_KeyboardInputEvent_1_2>()) {
    return get_interface<PPB_KeyboardInputEvent_1_2>()->GetKeyCode(
        pp_resource());
  } else if (has_interface<PPB_KeyboardInputEvent_1_0>()) {
    return get_interface<PPB_KeyboardInputEvent_1_0>()->GetKeyCode(
        pp_resource());
  }
  return 0;
}

Var KeyboardInputEvent::GetCharacterText() const {
  if (has_interface<PPB_KeyboardInputEvent_1_2>()) {
    return Var(PASS_REF,
               get_interface<PPB_KeyboardInputEvent_1_2>()->GetCharacterText(
                   pp_resource()));
  } else if (has_interface<PPB_KeyboardInputEvent_1_0>()) {
    return Var(PASS_REF,
               get_interface<PPB_KeyboardInputEvent_1_0>()->GetCharacterText(
                 pp_resource()));
  }
  return Var();
}

Var KeyboardInputEvent::GetCode() const {
  if (has_interface<PPB_KeyboardInputEvent_1_2>()) {
    return Var(PASS_REF,
               get_interface<PPB_KeyboardInputEvent_1_2>()->GetCode(
                   pp_resource()));
  }
  return Var();
}

// TouchInputEvent ------------------------------------------------------------
TouchInputEvent::TouchInputEvent() : InputEvent() {
}

TouchInputEvent::TouchInputEvent(const InputEvent& event) : InputEvent() {
  if (!has_interface<PPB_TouchInputEvent_1_0>())
    return;
  // Type check the input event before setting it.
  if (get_interface<PPB_TouchInputEvent_1_0>()->IsTouchInputEvent(
      event.pp_resource())) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

TouchInputEvent::TouchInputEvent(const InstanceHandle& instance,
                                 PP_InputEvent_Type type,
                                 PP_TimeTicks time_stamp,
                                 uint32_t modifiers) {
  // Type check the input event before setting it.
  if (!has_interface<PPB_TouchInputEvent_1_0>())
    return;
  PassRefFromConstructor(get_interface<PPB_TouchInputEvent_1_0>()->Create(
      instance.pp_instance(), type, time_stamp, modifiers));
}

void TouchInputEvent::AddTouchPoint(PP_TouchListType list,
                                    PP_TouchPoint point) {
  if (!has_interface<PPB_TouchInputEvent_1_0>())
    return;
  get_interface<PPB_TouchInputEvent_1_0>()->AddTouchPoint(pp_resource(), list,
                                                          &point);
}

uint32_t TouchInputEvent::GetTouchCount(PP_TouchListType list) const {
  if (!has_interface<PPB_TouchInputEvent_1_0>())
    return 0;
  return get_interface<PPB_TouchInputEvent_1_0>()->GetTouchCount(pp_resource(),
                                                                 list);
}

TouchPoint TouchInputEvent::GetTouchById(PP_TouchListType list,
                                             uint32_t id) const {
  if (!has_interface<PPB_TouchInputEvent_1_0>())
    return TouchPoint();

  if (has_interface<PPB_TouchInputEvent_1_4>()) {
    return TouchPoint(
        get_interface<PPB_TouchInputEvent_1_4>()->GetTouchById(pp_resource(),
                                                               list, id),
        get_interface<PPB_TouchInputEvent_1_4>()->GetTouchTiltById(
            pp_resource(), list, id));
  }

  return TouchPoint(get_interface<PPB_TouchInputEvent_1_0>()->GetTouchById(
      pp_resource(), list, id));
}

TouchPoint TouchInputEvent::GetTouchByIndex(PP_TouchListType list,
                                                uint32_t index) const {
  if (!has_interface<PPB_TouchInputEvent_1_0>())
    return TouchPoint();

  if (has_interface<PPB_TouchInputEvent_1_4>()) {
    return TouchPoint(
        get_interface<PPB_TouchInputEvent_1_4>()->GetTouchByIndex(pp_resource(),
                                                                  list, index),
        get_interface<PPB_TouchInputEvent_1_4>()->GetTouchTiltByIndex(
            pp_resource(), list, index));
  }

  return TouchPoint(get_interface<PPB_TouchInputEvent_1_0>()->
                        GetTouchByIndex(pp_resource(), list, index));
}

// IMEInputEvent -------------------------------------------------------

IMEInputEvent::IMEInputEvent() : InputEvent() {
}

IMEInputEvent::IMEInputEvent(const InputEvent& event) : InputEvent() {
  if (has_interface<PPB_IMEInputEvent_1_0>()) {
    if (get_interface<PPB_IMEInputEvent_1_0>()->IsIMEInputEvent(
            event.pp_resource())) {
      Module::Get()->core()->AddRefResource(event.pp_resource());
      PassRefFromConstructor(event.pp_resource());
    }
  }
}

IMEInputEvent::IMEInputEvent(
    const InstanceHandle& instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    const Var& text,
    const std::vector<uint32_t>& segment_offsets,
    int32_t target_segment,
    const std::pair<uint32_t, uint32_t>& selection) : InputEvent() {
  if (!has_interface<PPB_IMEInputEvent_1_0>())
    return;
  uint32_t dummy = 0;
  PassRefFromConstructor(get_interface<PPB_IMEInputEvent_1_0>()->Create(
      instance.pp_instance(), type, time_stamp, text.pp_var(),
      segment_offsets.empty() ? 0u :
          static_cast<uint32_t>(segment_offsets.size() - 1),
      segment_offsets.empty() ? &dummy : &segment_offsets[0],
      target_segment, selection.first, selection.second));
}


Var IMEInputEvent::GetText() const {
  if (has_interface<PPB_IMEInputEvent_1_0>()) {
    return Var(PASS_REF,
               get_interface<PPB_IMEInputEvent_1_0>()->GetText(
                   pp_resource()));
  }
  return Var();
}

uint32_t IMEInputEvent::GetSegmentNumber() const {
  if (has_interface<PPB_IMEInputEvent_1_0>()) {
    return get_interface<PPB_IMEInputEvent_1_0>()->GetSegmentNumber(
        pp_resource());
  }
  return 0;
}

uint32_t IMEInputEvent::GetSegmentOffset(uint32_t index) const {
  if (has_interface<PPB_IMEInputEvent_1_0>()) {
    return get_interface<PPB_IMEInputEvent_1_0>()->GetSegmentOffset(
        pp_resource(), index);
  }
  return 0;
}

int32_t IMEInputEvent::GetTargetSegment() const {
  if (has_interface<PPB_IMEInputEvent_1_0>()) {
    return get_interface<PPB_IMEInputEvent_1_0>()->GetTargetSegment(
        pp_resource());
  }
  return 0;
}

void IMEInputEvent::GetSelection(uint32_t* start, uint32_t* end) const {
  if (has_interface<PPB_IMEInputEvent_1_0>()) {
    get_interface<PPB_IMEInputEvent_1_0>()->GetSelection(pp_resource(),
                                                         start,
                                                         end);
  }
}

}  // namespace pp
