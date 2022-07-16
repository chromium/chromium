// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/input_event_conversions.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/var.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace chrome_pdf {

namespace {

blink::WebInputEvent::Type GetWebInputEventType(PP_InputEvent_Type event_type) {
  switch (event_type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      return blink::WebInputEvent::Type::kMouseDown;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      return blink::WebInputEvent::Type::kMouseUp;
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      return blink::WebInputEvent::Type::kMouseMove;
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      return blink::WebInputEvent::Type::kMouseEnter;
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      return blink::WebInputEvent::Type::kMouseLeave;
    case PP_INPUTEVENT_TYPE_WHEEL:
      return blink::WebInputEvent::Type::kMouseWheel;
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      return blink::WebInputEvent::Type::kRawKeyDown;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      return blink::WebInputEvent::Type::kKeyDown;
    case PP_INPUTEVENT_TYPE_KEYUP:
      return blink::WebInputEvent::Type::kKeyUp;
    case PP_INPUTEVENT_TYPE_CHAR:
      return blink::WebInputEvent::Type::kChar;
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      return blink::WebInputEvent::Type::kContextMenu;
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
      return blink::WebInputEvent::Type::kTouchStart;
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      return blink::WebInputEvent::Type::kTouchMove;
    case PP_INPUTEVENT_TYPE_TOUCHEND:
      return blink::WebInputEvent::Type::kTouchEnd;
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      return blink::WebInputEvent::Type::kTouchCancel;
    default:
      NOTREACHED();
      return blink::WebInputEvent::Type::kUndefined;
  }
}

blink::WebPointerProperties::Button GetWebPointerPropertiesButton(
    const PP_InputEvent_MouseButton& button_type) {
  switch (button_type) {
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      return blink::WebPointerProperties::Button::kLeft;
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      return blink::WebPointerProperties::Button::kMiddle;
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      return blink::WebPointerProperties::Button::kRight;
    default:
      // No other mouse button type is handled by the PDF plugin.
      return blink::WebPointerProperties::Button::kNoButton;
  }
}

std::unique_ptr<blink::WebMouseEvent> GetWebMouseEvent(
    const pp::MouseInputEvent& event) {
  const blink::WebInputEvent::Type type = GetWebInputEventType(event.GetType());
  DCHECK(blink::WebInputEvent::IsMouseEventType(type));
  DCHECK_NE(type, blink::WebInputEvent::Type::kContextMenu);

  auto mouse_event = std::make_unique<blink::WebMouseEvent>(
      type, event.GetModifiers(),
      base::TimeTicks() + base::Seconds(event.GetTimeStamp()));

  mouse_event->button = GetWebPointerPropertiesButton(event.GetButton());
  mouse_event->click_count = event.GetClickCount();

  const pp::Point& position = event.GetPosition();
  mouse_event->SetPositionInWidget(position.x(), position.y());

  return mouse_event;
}

std::unique_ptr<blink::WebKeyboardEvent> GetWebKeyboardEvent(
    const pp::KeyboardInputEvent& event) {
  const blink::WebInputEvent::Type type = GetWebInputEventType(event.GetType());
  DCHECK(blink::WebInputEvent::IsKeyboardEventType(type));

  auto keyboard_event = std::make_unique<blink::WebKeyboardEvent>(
      type, event.GetModifiers(),
      base::TimeTicks() + base::Seconds(event.GetTimeStamp()));

  keyboard_event->windows_key_code = event.GetKeyCode();

  const std::u16string text16 =
      base::UTF8ToUTF16(event.GetCharacterText().AsString());
  const size_t text_len =
      std::min(blink::WebKeyboardEvent::kTextLengthCap, text16.size());
  std::copy_n(text16.begin(), text_len, keyboard_event->text);
  std::fill_n(keyboard_event->text + text_len,
              blink::WebKeyboardEvent::kTextLengthCap - text_len, L'\0');

  return keyboard_event;
}

std::unique_ptr<blink::WebTouchEvent> GetWebTouchEvent(
    const pp::TouchInputEvent& event) {
  const blink::WebInputEvent::Type type = GetWebInputEventType(event.GetType());
  DCHECK(blink::WebInputEvent::IsTouchEventType(type));
  DCHECK_NE(type, blink::WebInputEvent::Type::kTouchScrollStarted);

  auto touch_event = std::make_unique<blink::WebTouchEvent>(
      type, event.GetModifiers(),
      base::TimeTicks() + base::Seconds(event.GetTimeStamp()));

  // The PDF plugin only cares about the first touch and the number of touches,
  // but copy over all the touches so that `touch_event->touches_length`
  // accurately stores the length of `touches`.
  touch_event->touches_length =
      std::min<uint32_t>(blink::WebTouchEvent::kTouchesLengthCap,
                         event.GetTouchCount(PP_TOUCHLIST_TYPE_TARGETTOUCHES));
  for (size_t i = 0; i < touch_event->touches_length; ++i) {
    touch_event->touches[i].SetPositionInWidget(PointFFromPPFloatPoint(
        event.GetTouchByIndex(PP_TOUCHLIST_TYPE_TARGETTOUCHES, i).position()));
  }

  return touch_event;
}

}  // namespace

std::unique_ptr<blink::WebInputEvent> GetWebInputEvent(
    const pp::InputEvent& event) {
  switch (GetWebInputEventType(event.GetType())) {
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kMouseUp:
    case blink::WebInputEvent::Type::kMouseMove:
    case blink::WebInputEvent::Type::kMouseEnter:
    case blink::WebInputEvent::Type::kMouseLeave:
      return GetWebMouseEvent(pp::MouseInputEvent(event));
    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown:
    case blink::WebInputEvent::Type::kKeyUp:
    case blink::WebInputEvent::Type::kChar:
      return GetWebKeyboardEvent(pp::KeyboardInputEvent(event));
    case blink::WebInputEvent::Type::kTouchStart:
    case blink::WebInputEvent::Type::kTouchMove:
    case blink::WebInputEvent::Type::kTouchEnd:
    case blink::WebInputEvent::Type::kTouchCancel:
      return GetWebTouchEvent(pp::TouchInputEvent(event));
    default:
      // Don't bother converting event types not handled by the PDF plugin.
      return nullptr;
  }
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
