// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_android.h"

#include <android/input.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "ui/events/android/key_event_utils.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

int WebInputEventToAndroidModifier(int web_modifier) {
  int android_modifier = 0;
  // Currently only Shift, CapsLock are used, add other modifiers if required.
  if (web_modifier & WebInputEvent::kShiftKey)
    android_modifier |= AMETA_SHIFT_ON;
  if (web_modifier & WebInputEvent::kCapsLockOn)
    android_modifier |= AMETA_CAPS_LOCK_ON;
  return android_modifier;
}

ui::DomKey GetDomKeyFromEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    int keycode,
    int modifiers,
    int unicode_character) {
  // Synthetic key event, not enough information to get DomKey.
  if (android_key_event.is_null() && !unicode_character)
    return ui::DomKey::UNIDENTIFIED;

  if (!unicode_character && env) {
    // According to spec |kAllowedModifiers| should be Shift and AltGr, however
    // Android doesn't have AltGr key and ImeAdapter::getModifiers won't pass it
    // either.
    // According to discussion we want to honor CapsLock and possibly NumLock as
    // well. https://github.com/w3c/uievents/issues/70
    const int kAllowedModifiers =
        WebInputEvent::kShiftKey | WebInputEvent::kCapsLockOn;
    int fallback_modifiers =
        WebInputEventToAndroidModifier(modifiers & kAllowedModifiers);

    unicode_character = ui::events::android::GetKeyEventUnicodeChar(
        env, android_key_event, fallback_modifiers);
  }

  ui::DomKey key = ui::GetDomKeyFromAndroidEvent(keycode, unicode_character);
  if (key != ui::DomKey::NONE)
    return key;
  return ui::DomKey::UNIDENTIFIED;
}

}  // namespace

WebKeyboardEvent WebKeyboardEventBuilder::Build(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    WebInputEvent::Type type,
    int modifiers,
    base::TimeTicks time,
    int keycode,
    int scancode,
    int unicode_character,
    bool is_system_key) {
  DCHECK(WebInputEvent::IsKeyboardEventType(type));

  ui::DomCode dom_code = ui::DomCode::NONE;
  if (scancode)
    dom_code = ui::KeycodeConverter::NativeKeycodeToDomCode(scancode);

  WebKeyboardEvent result(
      type, modifiers | ui::DomCodeToWebInputEventModifiers(dom_code), time);
  result.windows_key_code = ui::LocatedToNonLocatedKeyboardCode(
      ui::KeyboardCodeFromAndroidKeyCode(keycode));
  result.native_key_code = keycode;
  result.dom_code = static_cast<int>(dom_code);
  result.dom_key = GetDomKeyFromEvent(env, android_key_event, keycode,
                                      modifiers, unicode_character);
  result.unmodified_text[0] = unicode_character;
  if (result.windows_key_code == ui::VKEY_RETURN) {
    // This is the same behavior as GTK:
    // We need to treat the enter key as a key press of character \r. This
    // is apparently just how webkit handles it and what it expects.
    result.unmodified_text[0] = '\r';
  }
  result.text[0] = result.unmodified_text[0];
  result.is_system_key = is_system_key;

  return result;
}

WebMouseEvent WebMouseEventBuilder::Build(
    const ui::MotionEventAndroid& motion_event,
    WebInputEvent::Type type,
    int click_count,
    int action_button) {
  DCHECK(WebInputEvent::IsMouseEventType(type));
  int modifiers = motion_event.GetFlags();
  WebMouseEvent result(type, ui::EventFlagsToWebEventModifiers(modifiers),
                       motion_event.GetEventTime());

  result.SetPositionInWidget(motion_event.GetX(0), motion_event.GetY(0));
  result.SetPositionInScreen(motion_event.GetRawX(0), motion_event.GetRawY(0));

  result.click_count = click_count;

  int button = action_button;
  // For events other than MouseDown/Up, action_button is not defined. So we are
  // determining |button| value from |modifiers| as is done in other platforms.
  if (type != WebInputEvent::kMouseDown && type != WebInputEvent::kMouseUp) {
    if (modifiers & ui::EF_LEFT_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_PRIMARY;
    else if (modifiers & ui::EF_MIDDLE_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_TERTIARY;
    else if (modifiers & ui::EF_RIGHT_MOUSE_BUTTON)
      button = ui::MotionEvent::BUTTON_SECONDARY;
    else
      button = 0;
  }

  ui::SetWebPointerPropertiesFromMotionEventData(
      result, motion_event.GetPointerId(0), motion_event.GetPressure(0),
      motion_event.GetOrientation(0), motion_event.GetTiltX(0),
      motion_event.GetTiltY(0), motion_event.GetTwist(0),
      motion_event.GetTangentialPressure(0), button,
      motion_event.GetToolType(0));

  return result;
}

WebMouseWheelEvent WebMouseWheelEventBuilder::Build(
    const ui::MotionEventAndroid& motion_event) {
  WebMouseWheelEvent result(WebInputEvent::kMouseWheel,
                            WebInputEvent::kNoModifiers,
                            motion_event.GetEventTime());
  result.SetPositionInWidget(motion_event.GetX(0), motion_event.GetY(0));
  result.SetPositionInScreen(motion_event.GetRawX(0), motion_event.GetRawY(0));
  result.button = WebMouseEvent::Button::kNoButton;
  result.delta_units =
      ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
  result.delta_x = motion_event.ticks_x() * motion_event.GetTickMultiplier();
  result.delta_y = motion_event.ticks_y() * motion_event.GetTickMultiplier();
  result.wheel_ticks_x = motion_event.ticks_x();
  result.wheel_ticks_y = motion_event.ticks_y();

  return result;
}

WebGestureEvent WebGestureEventBuilder::Build(WebInputEvent::Type type,
                                              base::TimeTicks time,
                                              float x,
                                              float y) {
  DCHECK(WebInputEvent::IsGestureEventType(type));
  WebGestureEvent result(type, WebInputEvent::kNoModifiers, time,
                         blink::WebGestureDevice::kTouchscreen);
  result.SetPositionInWidget(gfx::PointF(x, y));

  return result;
}

}  // namespace content
