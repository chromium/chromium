// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "pdf/ppapi_migration/input_event_conversions.h"

#include "base/notreached.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/var.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace {

chrome_pdf::InputEventType GetEventType(const PP_InputEvent_Type& input_type) {
  switch (input_type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      return chrome_pdf::InputEventType::kMouseDown;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      return chrome_pdf::InputEventType::kMouseUp;
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      return chrome_pdf::InputEventType::kMouseMove;
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      return chrome_pdf::InputEventType::kMouseEnter;
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      return chrome_pdf::InputEventType::kMouseLeave;
    case PP_INPUTEVENT_TYPE_WHEEL:
      return chrome_pdf::InputEventType::kWheel;
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      return chrome_pdf::InputEventType::kRawKeyDown;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      return chrome_pdf::InputEventType::kKeyDown;
    case PP_INPUTEVENT_TYPE_KEYUP:
      return chrome_pdf::InputEventType::kKeyUp;
    case PP_INPUTEVENT_TYPE_CHAR:
      return chrome_pdf::InputEventType::kChar;
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      return chrome_pdf::InputEventType::kContextMenu;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
      return chrome_pdf::InputEventType::kImeCompositionStart;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
      return chrome_pdf::InputEventType::kImeCompositionUpdate;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
      return chrome_pdf::InputEventType::kImeCompositionEnd;
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      return chrome_pdf::InputEventType::kImeText;
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
      return chrome_pdf::InputEventType::kTouchStart;
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      return chrome_pdf::InputEventType::kTouchMove;
    case PP_INPUTEVENT_TYPE_TOUCHEND:
      return chrome_pdf::InputEventType::kTouchEnd;
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      return chrome_pdf::InputEventType::kTouchCancel;
    default:
      NOTREACHED();
      return chrome_pdf::InputEventType::kNone;
  }
}

chrome_pdf::InputEventMouseButtonType GetInputEventMouseButtonType(
    const PP_InputEvent_MouseButton& mouse_button_type) {
  switch (mouse_button_type) {
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      return chrome_pdf::InputEventMouseButtonType::kLeft;
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      return chrome_pdf::InputEventMouseButtonType::kMiddle;
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      return chrome_pdf::InputEventMouseButtonType::kRight;
    default:
      return chrome_pdf::InputEventMouseButtonType::kNone;
  }
}

bool IsKeyboardEventType(chrome_pdf::InputEventType event_type) {
  switch (event_type) {
    case chrome_pdf::InputEventType::kRawKeyDown:
    case chrome_pdf::InputEventType::kKeyDown:
    case chrome_pdf::InputEventType::kKeyUp:
    case chrome_pdf::InputEventType::kChar:
      return true;
    default:
      return false;
  }
}

bool IsMouseEventType(chrome_pdf::InputEventType event_type) {
  switch (event_type) {
    case chrome_pdf::InputEventType::kMouseDown:
    case chrome_pdf::InputEventType::kMouseUp:
    case chrome_pdf::InputEventType::kMouseMove:
    case chrome_pdf::InputEventType::kMouseEnter:
    case chrome_pdf::InputEventType::kMouseLeave:
      return true;
    default:
      return false;
  }
}

bool IsTouchEventType(chrome_pdf::InputEventType event_type) {
  switch (event_type) {
    case chrome_pdf::InputEventType::kTouchStart:
    case chrome_pdf::InputEventType::kTouchMove:
    case chrome_pdf::InputEventType::kTouchEnd:
    case chrome_pdf::InputEventType::kTouchCancel:
      return true;
    default:
      return false;
  }
}

}  // namespace

namespace chrome_pdf {

InputEvent::InputEvent(InputEventType event_type,
                       double time_stamp,
                       uint32_t modifiers)
    : event_type_(event_type), time_stamp_(time_stamp), modifiers_(modifiers) {}

InputEvent::InputEvent(const InputEvent& other) = default;

InputEvent& InputEvent::operator=(const InputEvent& other) = default;

InputEvent::~InputEvent() = default;

KeyboardInputEvent::KeyboardInputEvent(InputEventType event_type,
                                       double time_stamp,
                                       uint32_t modifiers,
                                       uint32_t keyboard_code,
                                       const std::string& key_char)
    : InputEvent(event_type, time_stamp, modifiers),
      keyboard_code_(keyboard_code),
      key_char_(key_char) {
  DCHECK(IsKeyboardEventType(GetEventType()));
}

KeyboardInputEvent::KeyboardInputEvent(const KeyboardInputEvent& other) =
    default;

KeyboardInputEvent& KeyboardInputEvent::operator=(
    const KeyboardInputEvent& other) = default;

KeyboardInputEvent::~KeyboardInputEvent() = default;

MouseInputEvent::MouseInputEvent(InputEventType event_type,
                                 double time_stamp,
                                 uint32_t modifiers,
                                 InputEventMouseButtonType mouse_button_type,
                                 const gfx::Point& point,
                                 int32_t click_count,
                                 const gfx::Point& movement)
    : InputEvent(event_type, time_stamp, modifiers),
      mouse_button_type_(mouse_button_type),
      point_(point),
      click_count_(click_count),
      movement_(movement) {
  DCHECK(IsMouseEventType(GetEventType()));
}

MouseInputEvent::MouseInputEvent(const MouseInputEvent& other) = default;

MouseInputEvent& MouseInputEvent::operator=(const MouseInputEvent& other) =
    default;

MouseInputEvent::~MouseInputEvent() = default;

TouchInputEvent::TouchInputEvent(InputEventType event_type,
                                 double time_stamp,
                                 uint32_t modifiers,
                                 const gfx::PointF& target_touch_point,
                                 int32_t touch_count)
    : InputEvent(event_type, time_stamp, modifiers),
      target_touch_point_(target_touch_point),
      touch_count_(touch_count) {
  DCHECK(IsTouchEventType(GetEventType()));
}

TouchInputEvent::TouchInputEvent(const TouchInputEvent& other) = default;

TouchInputEvent& TouchInputEvent::operator=(const TouchInputEvent& other) =
    default;

TouchInputEvent::~TouchInputEvent() = default;

NoneInputEvent::NoneInputEvent()
    : InputEvent(InputEventType::kNone, 0, kInputEventModifierNone) {}

NoneInputEvent::NoneInputEvent(const NoneInputEvent& other) = default;

NoneInputEvent& NoneInputEvent::operator=(const NoneInputEvent& other) =
    default;

NoneInputEvent::~NoneInputEvent() = default;

KeyboardInputEvent GetKeyboardInputEvent(const pp::KeyboardInputEvent& event) {
  return KeyboardInputEvent(GetEventType(event.GetType()), event.GetTimeStamp(),
                            event.GetModifiers(), event.GetKeyCode(),
                            event.GetCharacterText().AsString());
}

MouseInputEvent GetMouseInputEvent(const pp::MouseInputEvent& event) {
  return MouseInputEvent(
      GetEventType(event.GetType()), event.GetTimeStamp(), event.GetModifiers(),
      GetInputEventMouseButtonType(event.GetButton()),
      PointFromPPPoint(event.GetPosition()), event.GetClickCount(),
      PointFromPPPoint(event.GetMovement()));
}

TouchInputEvent GetTouchInputEvent(const pp::TouchInputEvent& event) {
  pp::FloatPoint point =
      event.GetTouchByIndex(PP_TOUCHLIST_TYPE_TARGETTOUCHES, 0).position();
  return TouchInputEvent(GetEventType(event.GetType()), event.GetTimeStamp(),
                         event.GetModifiers(), PointFFromPPFloatPoint(point),
                         event.GetTouchCount(PP_TOUCHLIST_TYPE_TARGETTOUCHES));
}

PP_CursorType_Dev PPCursorTypeFromCursorType(
    ui::mojom::CursorType cursor_type) {
  switch (cursor_type) {
    case ui::mojom::CursorType::kPointer:
      return PP_CURSORTYPE_POINTER;
    case ui::mojom::CursorType::kHand:
      return PP_CURSORTYPE_HAND;
    case ui::mojom::CursorType::kIBeam:
      return PP_CURSORTYPE_IBEAM;
    default:
      NOTREACHED();
      return PP_CURSORTYPE_POINTER;
  }
}

}  // namespace chrome_pdf
