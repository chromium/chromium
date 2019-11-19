// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/event_conversion.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/public/common/content_features.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

using ppapi::InputEventData;
using ppapi::TouchPointWithTilt;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebUChar;

namespace content {

namespace {

// Verify the modifier flags WebKit uses match the Pepper ones. If these start
// not matching, we'll need to write conversion code to preserve the Pepper
// values (since plugins will be depending on them).
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_SHIFTKEY) ==
                  static_cast<int>(WebInputEvent::kShiftKey),
              "ShiftKey should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_CONTROLKEY) ==
                  static_cast<int>(WebInputEvent::kControlKey),
              "ControlKey should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_ALTKEY) ==
                  static_cast<int>(WebInputEvent::kAltKey),
              "AltKey should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_METAKEY) ==
                  static_cast<int>(WebInputEvent::kMetaKey),
              "MetaKey should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_ISKEYPAD) ==
                  static_cast<int>(WebInputEvent::kIsKeyPad),
              "KeyPad should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT) ==
                  static_cast<int>(WebInputEvent::kIsAutoRepeat),
              "AutoRepeat should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) ==
                  static_cast<int>(WebInputEvent::kLeftButtonDown),
              "LeftButton should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN) ==
                  static_cast<int>(WebInputEvent::kMiddleButtonDown),
              "MiddleButton should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN) ==
                  static_cast<int>(WebInputEvent::kRightButtonDown),
              "RightButton should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY) ==
                  static_cast<int>(WebInputEvent::kCapsLockOn),
              "CapsLock should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_NUMLOCKKEY) ==
                  static_cast<int>(WebInputEvent::kNumLockOn),
              "NumLock should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_ISLEFT) ==
                  static_cast<int>(WebInputEvent::kIsLeft),
              "IsLeft should match");
static_assert(static_cast<int>(PP_INPUTEVENT_MODIFIER_ISRIGHT) ==
                  static_cast<int>(WebInputEvent::kIsRight),
              "IsRight should match");

PP_InputEvent_Type ConvertEventTypes(const WebInputEvent& event) {
  switch (event.GetType()) {
    case WebInputEvent::kMouseDown:
      return PP_INPUTEVENT_TYPE_MOUSEDOWN;
    case WebInputEvent::kMouseUp:
      return PP_INPUTEVENT_TYPE_MOUSEUP;
    case WebInputEvent::kMouseMove:
      return PP_INPUTEVENT_TYPE_MOUSEMOVE;
    case WebInputEvent::kMouseEnter:
      return PP_INPUTEVENT_TYPE_MOUSEENTER;
    case WebInputEvent::kMouseLeave:
      return PP_INPUTEVENT_TYPE_MOUSELEAVE;
    case WebInputEvent::kContextMenu:
      return PP_INPUTEVENT_TYPE_CONTEXTMENU;
    case WebInputEvent::kMouseWheel:
      return PP_INPUTEVENT_TYPE_WHEEL;
    case WebInputEvent::kRawKeyDown:
      // In the past blink has always returned kKeyDown passed into plugins
      // although PPAPI had a RAWKEYDOWN definition. However implementations are
      // broken now that blink passes kRawKeyDown so convert it to a keydown.
      return PP_INPUTEVENT_TYPE_KEYDOWN;
    case WebInputEvent::kKeyDown:
      return PP_INPUTEVENT_TYPE_KEYDOWN;
    case WebInputEvent::kKeyUp:
      return PP_INPUTEVENT_TYPE_KEYUP;
    case WebInputEvent::kChar:
      return PP_INPUTEVENT_TYPE_CHAR;
    case WebInputEvent::kTouchStart:
      return PP_INPUTEVENT_TYPE_TOUCHSTART;
    case WebInputEvent::kTouchMove:
      return PP_INPUTEVENT_TYPE_TOUCHMOVE;
    case WebInputEvent::kTouchEnd:
      return PP_INPUTEVENT_TYPE_TOUCHEND;
    case WebInputEvent::kTouchCancel:
      return PP_INPUTEVENT_TYPE_TOUCHCANCEL;
    case WebInputEvent::kUndefined:
    default:
      return PP_INPUTEVENT_TYPE_UNDEFINED;
  }
}

// Converts WebInputEvent::Modifiers flags to PP_InputEvent_Modifier.
int ConvertEventModifiers(int modifiers) {
  return modifiers & (PP_INPUTEVENT_MODIFIER_SHIFTKEY |
                      PP_INPUTEVENT_MODIFIER_CONTROLKEY |
                      PP_INPUTEVENT_MODIFIER_ALTKEY |
                      PP_INPUTEVENT_MODIFIER_METAKEY |
                      PP_INPUTEVENT_MODIFIER_ISKEYPAD |
                      PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT |
                      PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN |
                      PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN |
                      PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN |
                      PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY |
                      PP_INPUTEVENT_MODIFIER_NUMLOCKKEY |
                      PP_INPUTEVENT_MODIFIER_ISLEFT |
                      PP_INPUTEVENT_MODIFIER_ISRIGHT);
}

// Generates a PP_InputEvent with the fields common to all events, as well as
// the event type from the given web event. Event-specific fields will be zero
// initialized.
InputEventData GetEventWithCommonFieldsAndType(const WebInputEvent& web_event) {
  InputEventData result;
  result.event_type = ConvertEventTypes(web_event);
  result.event_time_stamp = web_event.TimeStamp().since_origin().InSecondsF();
  return result;
}

void AppendKeyEvent(const WebInputEvent& event,
                    std::vector<InputEventData>* result_events) {
  const WebKeyboardEvent& key_event =
      static_cast<const WebKeyboardEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers = ConvertEventModifiers(key_event.GetModifiers());
  result.key_code = key_event.windows_key_code;
  result.code = ui::KeycodeConverter::DomCodeToCodeString(
      static_cast<ui::DomCode>(key_event.dom_code));
  result_events->push_back(result);
}

void AppendCharEvent(const WebInputEvent& event,
                     std::vector<InputEventData>* result_events) {
  const WebKeyboardEvent& key_event =
      static_cast<const WebKeyboardEvent&>(event);

  // This is a bit complex, the input event will normally just have one 16-bit
  // character in it, but may be zero or more than one. The text array is
  // just padded with 0 values for the unused ones, but is not necessarily
  // null-terminated.
  //
  // Here we see how many UTF-16 characters we have.
  size_t utf16_char_count = 0;
  while (utf16_char_count < WebKeyboardEvent::kTextLengthCap &&
         key_event.text[utf16_char_count])
    utf16_char_count++;

  // Make a separate InputEventData for each Unicode character in the input.
  base::i18n::UTF16CharIterator iter(key_event.text, utf16_char_count);
  while (!iter.end()) {
    InputEventData result = GetEventWithCommonFieldsAndType(event);
    result.event_modifiers = ConvertEventModifiers(key_event.GetModifiers());
    base::WriteUnicodeCharacter(iter.get(), &result.character_text);

    result_events->push_back(result);
    iter.Advance();
  }
}

void AppendMouseEvent(const WebInputEvent& event,
                      std::unique_ptr<gfx::PointF>* in_out_last_mouse_position,
                      std::vector<InputEventData>* result_events) {
  static_assert(static_cast<int>(WebMouseEvent::Button::kNoButton) ==
                    static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_NONE),
                "MouseNone should match");
  static_assert(static_cast<int>(WebMouseEvent::Button::kLeft) ==
                    static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_LEFT),
                "MouseLeft should match");
  static_assert(static_cast<int>(WebMouseEvent::Button::kRight) ==
                    static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_RIGHT),
                "MouseRight should match");
  static_assert(static_cast<int>(WebMouseEvent::Button::kMiddle) ==
                    static_cast<int>(PP_INPUTEVENT_MOUSEBUTTON_MIDDLE),
                "MouseMiddle should match");

  const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers = ConvertEventModifiers(mouse_event.GetModifiers());
  if (mouse_event.GetType() == WebInputEvent::kMouseDown ||
      mouse_event.GetType() == WebInputEvent::kMouseMove ||
      mouse_event.GetType() == WebInputEvent::kMouseUp) {
    switch (mouse_event.button) {
      case WebMouseEvent::Button::kNoButton:
      case WebMouseEvent::Button::kLeft:
      case WebMouseEvent::Button::kRight:
      case WebMouseEvent::Button::kMiddle:
        result.mouse_button =
            static_cast<PP_InputEvent_MouseButton>(mouse_event.button);
        break;
      default:
        return;
    }
  }
  result.mouse_position.x = mouse_event.PositionInWidget().x;
  result.mouse_position.y = mouse_event.PositionInWidget().y;
  result.mouse_click_count = mouse_event.click_count;

  if (base::FeatureList::IsEnabled(features::kConsolidatedMovementXY)) {
    if (mouse_event.GetType() == WebInputEvent::kMouseMove &&
        *in_out_last_mouse_position) {
      result.mouse_movement.x =
          mouse_event.PositionInScreen().x - (*in_out_last_mouse_position)->x();
      result.mouse_movement.y =
          mouse_event.PositionInScreen().y - (*in_out_last_mouse_position)->y();
    }
    *in_out_last_mouse_position =
        std::make_unique<gfx::PointF>(mouse_event.PositionInScreen());

    // Filter out event generated by recentering the cursor when mouse locked.
    // See |RenderWidgetHostViewEventHandler::HandleMouseEventWhileLocked|.
    if ((mouse_event.GetModifiers() &
         WebInputEvent::Modifiers::kRelativeMotionEvent)) {
      return;
    }
  } else {
    result.mouse_movement.x = mouse_event.movement_x;
    result.mouse_movement.y = mouse_event.movement_y;
  }

  result_events->push_back(result);
}

void AppendMouseWheelEvent(const WebInputEvent& event,
                           std::vector<InputEventData>* result_events) {
  const WebMouseWheelEvent& mouse_wheel_event =
      static_cast<const WebMouseWheelEvent&>(event);
  InputEventData result = GetEventWithCommonFieldsAndType(event);
  result.event_modifiers =
      ConvertEventModifiers(mouse_wheel_event.GetModifiers());
  result.wheel_delta.x = mouse_wheel_event.delta_x;
  result.wheel_delta.y = mouse_wheel_event.delta_y;
  result.wheel_ticks.x = mouse_wheel_event.wheel_ticks_x;
  result.wheel_ticks.y = mouse_wheel_event.wheel_ticks_y;
  result.wheel_scroll_by_page =
      (mouse_wheel_event.delta_units ==
       ui::input_types::ScrollGranularity::kScrollByPage);
  result_events->push_back(result);
}

enum IncludedTouchPointTypes {
  ALL,     // All pointers targetting the plugin.
  ACTIVE,  // Only pointers that are currently down.
  CHANGED  // Only pointers that have changed since the previous event.
};
void SetPPTouchPoints(const WebTouchPoint* touches,
                      uint32_t touches_length,
                      IncludedTouchPointTypes included_types,
                      std::vector<TouchPointWithTilt>* result) {
  for (uint32_t i = 0; i < touches_length; i++) {
    const WebTouchPoint& touch_point = touches[i];
    if (included_types == ACTIVE &&
        (touch_point.state == WebTouchPoint::kStateReleased ||
         touch_point.state == WebTouchPoint::kStateCancelled)) {
      continue;
    }
    if (included_types == CHANGED &&
        (touch_point.state == WebTouchPoint::kStateUndefined ||
         touch_point.state == WebTouchPoint::kStateStationary)) {
      continue;
    }
    PP_TouchPoint pp_pt;
    pp_pt.id = touch_point.id;
    pp_pt.position.x = touch_point.PositionInWidget().x;
    pp_pt.position.y = touch_point.PositionInWidget().y;
    pp_pt.radius.x = touch_point.radius_x;
    pp_pt.radius.y = touch_point.radius_y;
    pp_pt.rotation_angle = touch_point.rotation_angle;
    pp_pt.pressure = touch_point.force;
    PP_FloatPoint pp_ft;
    pp_ft.x = touch_point.tilt_x;
    pp_ft.y = touch_point.tilt_y;
    TouchPointWithTilt touch_with_tilt{pp_pt, pp_ft};
    result->push_back(touch_with_tilt);
  }
}

void AppendTouchEvent(const WebInputEvent& event,
                      std::vector<InputEventData>* result_events) {
  const WebTouchEvent& touch_event =
      reinterpret_cast<const WebTouchEvent&>(event);

  InputEventData result = GetEventWithCommonFieldsAndType(event);

  if (touch_event.touches_length == 1) {
    if (touch_event.touches[0].pointer_type ==
        blink::WebPointerProperties::PointerType::kPen) {
      result.event_modifiers |= PP_INPUTEVENT_MODIFIER_ISPEN;
    } else if (touch_event.touches[0].pointer_type ==
               blink::WebPointerProperties::PointerType::kEraser) {
      result.event_modifiers |= PP_INPUTEVENT_MODIFIER_ISERASER;
    }
  }

  SetPPTouchPoints(touch_event.touches, touch_event.touches_length, ACTIVE,
                   &result.touches);
  SetPPTouchPoints(touch_event.touches, touch_event.touches_length, CHANGED,
                   &result.changed_touches);
  SetPPTouchPoints(touch_event.touches, touch_event.touches_length, ALL,
                   &result.target_touches);

  result_events->push_back(result);
}

WebTouchPoint CreateWebTouchPoint(const PP_TouchPoint& pp_pt,
                                  WebTouchPoint::State state) {
  WebTouchPoint pt;
  pt.pointer_type = blink::WebPointerProperties::PointerType::kTouch;
  pt.id = pp_pt.id;
  pt.SetPositionInWidget(pp_pt.position.x, pp_pt.position.y);
  // TODO(crbug.com/93902): Add screen coordinate calculation.
  pt.SetPositionInScreen(0, 0);
  pt.force = pp_pt.pressure;
  pt.radius_x = pp_pt.radius.x;
  pt.radius_y = pp_pt.radius.y;
  pt.rotation_angle = pp_pt.rotation_angle;
  pt.state = state;
  return pt;
}

bool HasTouchPointWithId(const WebTouchPoint* web_touches,
                         uint32_t web_touches_length,
                         uint32_t id) {
  // Note: A brute force search to find the (potentially) existing touch point
  // is cheap given the small bound on |WebTouchEvent::kTouchesLengthCap|.
  for (uint32_t i = 0; i < web_touches_length; ++i) {
    if (web_touches[i].id == static_cast<int>(id))
      return true;
  }
  return false;
}

void SetWebTouchPointsIfNotYetSet(
    const std::vector<TouchPointWithTilt>& pp_touches,
    WebTouchPoint::State state,
    WebTouchPoint* web_touches,
    uint32_t* web_touches_length) {
  const uint32_t initial_web_touches_length = *web_touches_length;
  const uint32_t touches_length =
      std::min(static_cast<uint32_t>(pp_touches.size()),
               static_cast<uint32_t>(WebTouchEvent::kTouchesLengthCap));
  for (uint32_t i = 0; i < touches_length; ++i) {
    const uint32_t touch_index = *web_touches_length;
    if (touch_index >= static_cast<uint32_t>(WebTouchEvent::kTouchesLengthCap))
      return;

    const PP_TouchPoint& pp_pt = pp_touches[i].touch;
    if (HasTouchPointWithId(web_touches, initial_web_touches_length, pp_pt.id))
      continue;

    web_touches[touch_index] = CreateWebTouchPoint(pp_pt, state);
    ++(*web_touches_length);
  }
}

WebTouchEvent* BuildTouchEvent(const InputEventData& event) {
  WebTouchEvent* web_event = new WebTouchEvent();
  WebTouchPoint::State state = WebTouchPoint::kStateUndefined;
  WebInputEvent::Type type = WebInputEvent::kUndefined;
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
      type = WebInputEvent::kTouchStart;
      state = WebTouchPoint::kStatePressed;
      break;
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      type = WebInputEvent::kTouchMove;
      state = WebTouchPoint::kStateMoved;
      break;
    case PP_INPUTEVENT_TYPE_TOUCHEND:
      type = WebInputEvent::kTouchEnd;
      state = WebTouchPoint::kStateReleased;
      break;
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      type = WebInputEvent::kTouchCancel;
      state = WebTouchPoint::kStateCancelled;
      break;
    default:
      NOTREACHED();
  }
  WebTouchEventTraits::ResetType(
      type,
      base::TimeTicks() + base::TimeDelta::FromSecondsD(event.event_time_stamp),
      web_event);
  web_event->touches_length = 0;

  // First add all changed touches, then add only the remaining unset
  // (stationary) touches.
  SetWebTouchPointsIfNotYetSet(event.changed_touches, state, web_event->touches,
                               &web_event->touches_length);
  SetWebTouchPointsIfNotYetSet(event.touches, WebTouchPoint::kStateStationary,
                               web_event->touches, &web_event->touches_length);

  return web_event;
}

WebKeyboardEvent* BuildKeyEvent(const InputEventData& event) {
  WebInputEvent::Type type = WebInputEvent::Type::kUndefined;
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      type = WebInputEvent::kRawKeyDown;
      break;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      type = WebInputEvent::kKeyDown;
      break;
    case PP_INPUTEVENT_TYPE_KEYUP:
      type = WebInputEvent::kKeyUp;
      break;
    default:
      NOTREACHED();
  }
  WebKeyboardEvent* key_event = new WebKeyboardEvent(
      type, event.event_modifiers,
      base::TimeTicks() +
          base::TimeDelta::FromSecondsD(event.event_time_stamp));
  key_event->windows_key_code = event.key_code;
  return key_event;
}

WebKeyboardEvent* BuildCharEvent(const InputEventData& event) {
  WebKeyboardEvent* key_event = new WebKeyboardEvent(
      WebInputEvent::kChar, event.event_modifiers,
      base::TimeTicks() +
          base::TimeDelta::FromSecondsD(event.event_time_stamp));

  // Make sure to not read beyond the buffer in case some bad code doesn't
  // NULL-terminate it (this is called from plugins).
  size_t text_length_cap = WebKeyboardEvent::kTextLengthCap;
  base::string16 text16 = base::UTF8ToUTF16(event.character_text);

  std::fill_n(key_event->text, text_length_cap, 0);
  std::fill_n(key_event->unmodified_text, text_length_cap, 0);
  for (size_t i = 0; i < std::min(text_length_cap, text16.size()); ++i)
    key_event->text[i] = text16[i];
  return key_event;
}

WebMouseEvent* BuildMouseEvent(const InputEventData& event) {
  WebInputEvent::Type type = WebInputEvent::kUndefined;
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      type = WebInputEvent::kMouseDown;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      type = WebInputEvent::kMouseUp;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      type = WebInputEvent::kMouseMove;
      break;
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      type = WebInputEvent::kMouseEnter;
      break;
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      type = WebInputEvent::kMouseLeave;
      break;
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      type = WebInputEvent::kContextMenu;
      break;
    default:
      NOTREACHED();
  }
  WebMouseEvent* mouse_event = new WebMouseEvent(
      type, event.event_modifiers,
      base::TimeTicks() +
          base::TimeDelta::FromSecondsD(event.event_time_stamp));
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event->button = static_cast<WebMouseEvent::Button>(event.mouse_button);
  if (mouse_event->GetType() == WebInputEvent::kMouseMove) {
    if (mouse_event->GetModifiers() & WebInputEvent::kLeftButtonDown)
      mouse_event->button = WebMouseEvent::Button::kLeft;
    else if (mouse_event->GetModifiers() & WebInputEvent::kMiddleButtonDown)
      mouse_event->button = WebMouseEvent::Button::kMiddle;
    else if (mouse_event->GetModifiers() & WebInputEvent::kRightButtonDown)
      mouse_event->button = WebMouseEvent::Button::kRight;
  }
  mouse_event->SetPositionInWidget(event.mouse_position.x,
                                   event.mouse_position.y);
  mouse_event->click_count = event.mouse_click_count;
  mouse_event->movement_x = event.mouse_movement.x;
  mouse_event->movement_y = event.mouse_movement.y;
  return mouse_event;
}

WebMouseWheelEvent* BuildMouseWheelEvent(const InputEventData& event) {
  WebMouseWheelEvent* mouse_wheel_event = new WebMouseWheelEvent(
      WebInputEvent::kMouseWheel, event.event_modifiers,
      base::TimeTicks() +
          base::TimeDelta::FromSecondsD(event.event_time_stamp));
  mouse_wheel_event->delta_x = event.wheel_delta.x;
  mouse_wheel_event->delta_y = event.wheel_delta.y;
  mouse_wheel_event->wheel_ticks_x = event.wheel_ticks.x;
  mouse_wheel_event->wheel_ticks_y = event.wheel_ticks.y;
  mouse_wheel_event->delta_units =
      event.wheel_scroll_by_page
          ? ui::input_types::ScrollGranularity::kScrollByPage
          : ui::input_types::ScrollGranularity::kScrollByPixel;
  return mouse_wheel_event;
}

#if !defined(OS_WIN)
#define VK_RETURN 0x0D

#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E

#define VK_APPS 0x5D

#define VK_F1 0x70
#endif

// Convert a character string to a Windows virtual key code. Adapted from
// src/content/shell/test_runner/event_sender.cc. This
// is used by CreateSimulatedWebInputEvents to convert keyboard events.
void GetKeyCode(const std::string& char_text,
                WebUChar* code,
                WebUChar* text,
                bool* needs_shift_modifier,
                bool* generate_char) {
  WebUChar vk_code = 0;
  WebUChar vk_text = 0;
  *needs_shift_modifier = false;
  *generate_char = false;
  if ("\n" == char_text) {
    vk_text = vk_code = VK_RETURN;
    *generate_char = true;
  } else if ("rightArrow" == char_text) {
    vk_code = VK_RIGHT;
  } else if ("downArrow" == char_text) {
    vk_code = VK_DOWN;
  } else if ("leftArrow" == char_text) {
    vk_code = VK_LEFT;
  } else if ("upArrow" == char_text) {
    vk_code = VK_UP;
  } else if ("insert" == char_text) {
    vk_code = VK_INSERT;
  } else if ("delete" == char_text) {
    vk_code = VK_DELETE;
  } else if ("pageUp" == char_text) {
    vk_code = VK_PRIOR;
  } else if ("pageDown" == char_text) {
    vk_code = VK_NEXT;
  } else if ("home" == char_text) {
    vk_code = VK_HOME;
  } else if ("end" == char_text) {
    vk_code = VK_END;
  } else if ("printScreen" == char_text) {
    vk_code = VK_SNAPSHOT;
  } else if ("menu" == char_text) {
    vk_code = VK_APPS;
  } else {
    // Compare the input string with the function-key names defined by the
    // DOM spec (i.e. "F1",...,"F24").
    for (int i = 1; i <= 24; ++i) {
      std::string functionKeyName = base::StringPrintf("F%d", i);
      if (functionKeyName == char_text) {
        vk_code = VK_F1 + (i - 1);
        break;
      }
    }
    if (!vk_code) {
      base::string16 char_text16 = base::UTF8ToUTF16(char_text);
      DCHECK_EQ(char_text16.size(), 1U);
      vk_text = vk_code = char_text16[0];
      *needs_shift_modifier = base::IsAsciiUpper(vk_code & 0xFF);
      if (base::IsAsciiLower(vk_code & 0xFF))
        vk_code -= 'a' - 'A';
      *generate_char = true;
    }
  }

  *code = vk_code;
  *text = vk_text;
}

}  // namespace

void CreateInputEventData(
    const WebInputEvent& event,
    std::unique_ptr<gfx::PointF>* in_out_last_mouse_position,
    std::vector<InputEventData>* result) {
  result->clear();

  switch (event.GetType()) {
    case WebInputEvent::kMouseDown:
    case WebInputEvent::kMouseUp:
    case WebInputEvent::kMouseMove:
    case WebInputEvent::kMouseEnter:
    case WebInputEvent::kMouseLeave:
    case WebInputEvent::kContextMenu:
      AppendMouseEvent(event, in_out_last_mouse_position, result);
      break;
    case WebInputEvent::kMouseWheel:
      AppendMouseWheelEvent(event, result);
      break;
    case WebInputEvent::kRawKeyDown:
    case WebInputEvent::kKeyDown:
    case WebInputEvent::kKeyUp:
      AppendKeyEvent(event, result);
      break;
    case WebInputEvent::kChar:
      AppendCharEvent(event, result);
      break;
    case WebInputEvent::kTouchStart:
    case WebInputEvent::kTouchMove:
    case WebInputEvent::kTouchEnd:
    case WebInputEvent::kTouchCancel:
      AppendTouchEvent(event, result);
      break;
    case WebInputEvent::kUndefined:
    default:
      break;
  }
}

WebInputEvent* CreateWebInputEvent(const InputEventData& event) {
  std::unique_ptr<WebInputEvent> web_input_event;
  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      return nullptr;
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      web_input_event.reset(BuildMouseEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_WHEEL:
      web_input_event.reset(BuildMouseWheelEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
      web_input_event.reset(BuildKeyEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_CHAR:
      web_input_event.reset(BuildCharEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      // TODO(kinaba) implement in WebKit an event structure to handle
      // composition events.
      NOTREACHED();
      break;
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      web_input_event.reset(BuildTouchEvent(event));
      break;
  }

  return web_input_event.release();
}

// Generate a coherent sequence of input events to simulate a user event.
// From src/content/shell/test_runner/event_sender.cc.
std::vector<std::unique_ptr<WebInputEvent>> CreateSimulatedWebInputEvents(
    const ppapi::InputEventData& event,
    int plugin_x,
    int plugin_y) {
  std::vector<std::unique_ptr<WebInputEvent>> events;
  std::unique_ptr<WebInputEvent> original_event(CreateWebInputEvent(event));

  switch (event.event_type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      events.push_back(std::move(original_event));
      break;
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL: {
      blink::WebTouchEvent* touch_event =
          static_cast<blink::WebTouchEvent*>(original_event.get());
      for (unsigned i = 0; i < touch_event->touches_length; ++i) {
        const blink::WebTouchPoint& touch_point = touch_event->touches[i];
        if (touch_point.state != blink::WebTouchPoint::kStateStationary) {
          events.push_back(
              std::make_unique<WebPointerEvent>(*touch_event, touch_point));
        }
      }
    } break;
    case PP_INPUTEVENT_TYPE_WHEEL: {
      WebMouseWheelEvent* web_mouse_wheel_event =
          static_cast<WebMouseWheelEvent*>(original_event.get());
      web_mouse_wheel_event->SetPositionInWidget(plugin_x, plugin_y);
      events.push_back(std::move(original_event));
      break;
    }

    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP: {
// Windows key down events should always be "raw" to avoid an ASSERT.
#if defined(OS_WIN)
      WebKeyboardEvent* web_keyboard_event =
          static_cast<WebKeyboardEvent*>(original_event.get());
      if (web_keyboard_event->GetType() == WebInputEvent::kKeyDown)
        web_keyboard_event->SetType(WebInputEvent::kRawKeyDown);
#endif
      events.push_back(std::move(original_event));
      break;
    }

    case PP_INPUTEVENT_TYPE_CHAR: {
      WebKeyboardEvent* web_char_event =
          static_cast<WebKeyboardEvent*>(original_event.get());

      WebUChar code = 0, text = 0;
      bool needs_shift_modifier = false, generate_char = false;
      GetKeyCode(event.character_text,
                 &code,
                 &text,
                 &needs_shift_modifier,
                 &generate_char);

      // Synthesize key down and key up events in all cases.
      std::unique_ptr<WebKeyboardEvent> key_down_event(new WebKeyboardEvent(
          WebInputEvent::kRawKeyDown,
          needs_shift_modifier ? WebInputEvent::kShiftKey
                               : WebInputEvent::kNoModifiers,
          web_char_event->TimeStamp()));
      std::unique_ptr<WebKeyboardEvent> key_up_event(new WebKeyboardEvent());

      key_down_event->windows_key_code = code;
      key_down_event->native_key_code = code;

      // If a char event is needed, set the text fields.
      if (generate_char) {
        key_down_event->text[0] = text;
        key_down_event->unmodified_text[0] = text;
      }

      *key_up_event = *web_char_event = *key_down_event;

      events.push_back(std::move(key_down_event));

      if (generate_char) {
        web_char_event->SetType(WebInputEvent::kChar);
        events.push_back(std::move(original_event));
      }

      key_up_event->SetType(WebInputEvent::kKeyUp);
      events.push_back(std::move(key_up_event));
      break;
    }

    default:
      break;
  }
  return events;
}

PP_InputEvent_Class ClassifyInputEvent(const WebInputEvent& event) {
  switch (event.GetType()) {
    case WebInputEvent::kMouseDown:
    case WebInputEvent::kMouseUp:
    case WebInputEvent::kMouseMove:
    case WebInputEvent::kMouseEnter:
    case WebInputEvent::kMouseLeave:
    case WebInputEvent::kContextMenu:
      return PP_INPUTEVENT_CLASS_MOUSE;
    case WebInputEvent::kMouseWheel:
      return PP_INPUTEVENT_CLASS_WHEEL;
    case WebInputEvent::kRawKeyDown:
    case WebInputEvent::kKeyDown:
    case WebInputEvent::kKeyUp:
    case WebInputEvent::kChar:
      return PP_INPUTEVENT_CLASS_KEYBOARD;
    case WebInputEvent::kTouchCancel:
    case WebInputEvent::kTouchEnd:
    case WebInputEvent::kTouchMove:
    case WebInputEvent::kTouchStart:
      return PP_INPUTEVENT_CLASS_TOUCH;
    case WebInputEvent::kTouchScrollStarted:
      return PP_InputEvent_Class(0);
    default:
      CHECK(WebInputEvent::IsGestureEventType(event.GetType()));
      return PP_InputEvent_Class(0);
  }
}

}  // namespace content
