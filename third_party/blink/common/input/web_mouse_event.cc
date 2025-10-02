// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_mouse_event.h"

#include "third_party/blink/public/common/input/web_gesture_event.h"

namespace blink {

WebMouseEvent::WebMouseEvent(WebInputEvent::Type type,
                             const WebGestureEvent& gesture_event,
                             Button button_param,
                             int click_count_param,
                             int modifiers,
                             base::TimeTicks time_stamp,
                             PointerId id_param)
    : WebInputEvent(type,
                    WebInputEvent::Type::kMouseTypeFirst,
                    WebInputEvent::Type::kMouseTypeLast,
                    modifiers,
                    time_stamp),
      WebPointerProperties(id_param,
                           WebPointerProperties::PointerType::kMouse,
                           button_param),
      click_count(click_count_param) {
  SetPositionInWidget(gesture_event.PositionInWidget());
  SetPositionInScreen(gesture_event.PositionInScreen());
  SetFrameScale(gesture_event.FrameScale());
  SetFrameTranslate(gesture_event.FrameTranslate());
  SetMenuSourceType(gesture_event.GetType());
}

gfx::PointF WebMouseEvent::PositionInRootFrame() const {
  return gfx::ScalePoint(position_in_widget_, 1 / frame_scale_) +
         frame_translate_;
}

std::unique_ptr<WebInputEvent> WebMouseEvent::Clone() const {
  return std::make_unique<WebMouseEvent>(*this);
}

bool WebMouseEvent::CanCoalesce(const WebInputEvent& event) const {
  if (!IsMouseEventType(event.GetType()))
    return false;
  const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
  // Since we start supporting the stylus input and they are constructed as
  // mouse events or touch events, we should check the ID and pointer type when
  // coalescing mouse events.
  return GetType() == WebInputEvent::Type::kMouseMove &&
         GetType() == mouse_event.GetType() &&
         GetModifiers() == mouse_event.GetModifiers() && id == mouse_event.id &&
         pointer_type == mouse_event.pointer_type;
}

void WebMouseEvent::Coalesce(const WebInputEvent& event) {
  DCHECK(CanCoalesce(event));
  const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
  // Accumulate movement deltas.
  int x = movement_x;
  int y = movement_y;
  *this = mouse_event;
  movement_x += x;
  movement_y += y;
}

WebMouseEvent WebMouseEvent::FlattenTransform() const {
  WebMouseEvent result = *this;
  result.FlattenTransformSelf();
  return result;
}

void WebMouseEvent::FlattenTransformSelf() {
  position_in_widget_ = PositionInRootFrame();
  frame_translate_ = gfx::Vector2dF();
  frame_scale_ = 1;
}

void WebMouseEvent::SetMenuSourceType(WebInputEvent::Type type) {
  switch (type) {
    case Type::kGestureShortPress:
    case Type::kGestureTapDown:
    case Type::kGestureTap:
    case Type::kGestureDoubleTap:
      menu_source_type = kMenuSourceTouch;
      break;
    case Type::kGestureLongPress:
      menu_source_type = kMenuSourceLongPress;
      break;
    case Type::kGestureLongTap:
      menu_source_type = kMenuSourceLongTap;
      break;
    default:
      menu_source_type = kMenuSourceNone;
  }
}

void WebMouseEvent::UpdateEventModifiersToMatchButton() {
  unsigned button_modifier_bit = WebInputEvent::kNoModifiers;

  switch (button) {
    case blink::WebPointerProperties::Button::kNoButton:
      button_modifier_bit = WebInputEvent::kNoModifiers;
      break;

    case blink::WebPointerProperties::Button::kLeft:
      button_modifier_bit = WebInputEvent::kLeftButtonDown;
      break;

    case blink::WebPointerProperties::Button::kMiddle:
      button_modifier_bit = WebInputEvent::kMiddleButtonDown;
      break;

    case blink::WebPointerProperties::Button::kRight:
      button_modifier_bit = WebInputEvent::kRightButtonDown;
      break;

    case blink::WebPointerProperties::Button::kBack:
      button_modifier_bit = WebInputEvent::kBackButtonDown;
      break;

    case blink::WebPointerProperties::Button::kForward:
      button_modifier_bit = WebInputEvent::kForwardButtonDown;
      break;

    case blink::WebPointerProperties::Button::kEraser:
      // TODO(mustaq): WebInputEvent modifier needs to support stylus eraser
      // buttons.
      button_modifier_bit = WebInputEvent::kNoModifiers;
      break;
  }

  if (GetType() == WebInputEvent::Type::kMouseDown) {
    SetModifiers(GetModifiers() | button_modifier_bit);
  } else if (GetType() == WebInputEvent::Type::kMouseUp) {
    SetModifiers(GetModifiers() & ~button_modifier_bit);
  }
}

}  // namespace blink
