/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"

#include <array>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/input/touch.h"
#include "third_party/blink/renderer/core/input/touch_list.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

namespace {
float FrameScale(const LocalFrameView* frame_view) {
  float scale = 1;
  if (frame_view) {
    LocalFrameView* root_view = frame_view->GetFrame().LocalFrameRoot().View();
    if (root_view)
      scale = root_view->InputEventsScaleFactor();
  }
  return scale;
}

gfx::Vector2dF FrameTranslation(const LocalFrameView* frame_view) {
  gfx::Point visual_viewport;
  gfx::Vector2dF overscroll_offset;
  if (frame_view) {
    LocalFrameView* root_view = frame_view->GetFrame().LocalFrameRoot().View();
    if (root_view) {
      visual_viewport = gfx::ToFlooredPoint(
          root_view->GetPage()->GetVisualViewport().VisibleRect().origin());
      overscroll_offset =
          root_view->GetPage()->GetChromeClient().ElasticOverscroll();
    }
  }
  return visual_viewport.OffsetFromOrigin() + overscroll_offset;
}

void UpdateWebMouseEventFromCoreMouseEvent(const MouseEvent& event,
                                           const LayoutObject* layout_object,
                                           WebMouseEvent& web_event) {
  web_event.SetTimeStamp(event.PlatformTimeStamp());
  web_event.SetModifiers(event.GetModifiers());
  web_event.SetPositionInScreen(event.screenX(), event.screenY());

  gfx::PointF local_point = layout_object->AbsoluteToLocalPoint(
      gfx::PointF(event.AbsoluteLocation()));
  web_event.SetPositionInWidget(local_point);
}

unsigned ToWebInputEventModifierFrom(WebMouseEvent::Button button) {
  if (button == WebMouseEvent::Button::kNoButton)
    return 0;

  constexpr auto web_mouse_button_to_platform_modifier = std::to_array<unsigned>(
      {WebInputEvent::kLeftButtonDown, WebInputEvent::kMiddleButtonDown,
       WebInputEvent::kRightButtonDown, WebInputEvent::kBackButtonDown,
       WebInputEvent::kForwardButtonDown});

  return web_mouse_button_to_platform_modifier[static_cast<int>(button)];
}

WebPointerEvent TransformWebPointerEvent(float frame_scale,
                                         gfx::Vector2dF frame_translate,
                                         const WebPointerEvent& event) {
  // frameScale is default initialized to 1.
  DCHECK_EQ(1, event.FrameScale());
  DCHECK_EQ(0, event.FrameTranslate().x());
  DCHECK_EQ(0, event.FrameTranslate().y());
  WebPointerEvent result = event;
  result.SetFrameScale(frame_scale);
  result.SetFrameTranslate(frame_translate);
  return result;
}

}  // namespace

WebMouseEvent TransformWebMouseEvent(LocalFrameView* frame_view,
                                     const WebMouseEvent& event) {
  WebMouseEvent result = event;

  // TODO(dtapuska): Perhaps the event should be constructed correctly?
  // crbug.com/686200
  if (event.GetType() == WebInputEvent::Type::kMouseUp) {
    result.SetModifiers(event.GetModifiers() &
                        ~ToWebInputEventModifierFrom(event.button));
  }
  result.SetFrameScale(FrameScale(frame_view));
  result.SetFrameTranslate(FrameTranslation(frame_view));
  return result;
}

WebMouseWheelEvent TransformWebMouseWheelEvent(
    LocalFrameView* frame_view,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent result = event;
  result.SetFrameScale(FrameScale(frame_view));
  result.SetFrameTranslate(FrameTranslation(frame_view));
  return result;
}

WebGestureEvent TransformWebGestureEvent(LocalFrameView* frame_view,
                                         const WebGestureEvent& event) {
  WebGestureEvent result = event;
  result.SetFrameScale(FrameScale(frame_view));
  result.SetFrameTranslate(FrameTranslation(frame_view));
  return result;
}

WebPointerEvent TransformWebPointerEvent(LocalFrameView* frame_view,
                                         const WebPointerEvent& event) {
  return TransformWebPointerEvent(FrameScale(frame_view),
                                  FrameTranslation(frame_view), event);
}

WebMouseEventBuilder::WebMouseEventBuilder(const LayoutObject* layout_object,
                                           const MouseEvent& event) {
  // Code below here can be removed once OOPIF ships.
  // OOPIF will prevent synthetic events being dispatched into
  // other frames; but for now we allow the fallback to generate
  // WebMouseEvents from synthetic events.
  if (event.type() == event_type_names::kMousemove)
    type_ = WebInputEvent::Type::kMouseMove;
  else if (event.type() == event_type_names::kMouseout)
    type_ = WebInputEvent::Type::kMouseLeave;
  else if (event.type() == event_type_names::kMouseover)
    type_ = WebInputEvent::Type::kMouseEnter;
  else if (event.type() == event_type_names::kMousedown)
    type_ = WebInputEvent::Type::kMouseDown;
  else if (event.type() == event_type_names::kMouseup)
    type_ = WebInputEvent::Type::kMouseUp;
  else if (event.type() == event_type_names::kContextmenu)
    type_ = WebInputEvent::Type::kContextMenu;
  else
    return;  // Skip all other mouse events.

  time_stamp_ = event.PlatformTimeStamp();
  modifiers_ = event.GetModifiers();
  UpdateWebMouseEventFromCoreMouseEvent(event, layout_object, *this);

  switch (event.button()) {
    case int16_t(WebPointerProperties::Button::kLeft):
      button = WebMouseEvent::Button::kLeft;
      break;
    case int16_t(WebPointerProperties::Button::kMiddle):
      button = WebMouseEvent::Button::kMiddle;
      break;
    case int16_t(WebPointerProperties::Button::kRight):
      button = WebMouseEvent::Button::kRight;
      break;
    case int16_t(WebPointerProperties::Button::kBack):
      button = WebMouseEvent::Button::kBack;
      break;
    case int16_t(WebPointerProperties::Button::kForward):
      button = WebMouseEvent::Button::kForward;
      break;
  }
  if (event.ButtonDown()) {
    switch (event.button()) {
      case int16_t(WebPointerProperties::Button::kLeft):
        modifiers_ |= WebInputEvent::kLeftButtonDown;
        break;
      case int16_t(WebPointerProperties::Button::kMiddle):
        modifiers_ |= WebInputEvent::kMiddleButtonDown;
        break;
      case int16_t(WebPointerProperties::Button::kRight):
        modifiers_ |= WebInputEvent::kRightButtonDown;
        break;
      case int16_t(WebPointerProperties::Button::kBack):
        modifiers_ |= WebInputEvent::kBackButtonDown;
        break;
      case int16_t(WebPointerProperties::Button::kForward):
        modifiers_ |= WebInputEvent::kForwardButtonDown;
        break;
    }
  } else {
    button = WebMouseEvent::Button::kNoButton;
  }
  movement_x = event.movementX();
  movement_y = event.movementY();
  click_count = event.detail();

  pointer_type = WebPointerProperties::PointerType::kMouse;
}

// Generate a synthetic WebMouseEvent given a TouchEvent (eg. for emulating a
// mouse with touch input for plugins that don't support touch input).
WebMouseEventBuilder::WebMouseEventBuilder(const LayoutObject* layout_object,
                                           const TouchEvent& event) {
  if (!event.touches())
    return;
  if (event.touches()->length() != 1) {
    if (event.touches()->length() ||
        event.type() != event_type_names::kTouchend ||
        !event.changedTouches() || event.changedTouches()->length() != 1)
      return;
  }

  const Touch* touch = event.touches()->length() == 1
                           ? event.touches()->item(0)
                           : event.changedTouches()->item(0);
  if (touch->identifier())
    return;

  if (event.type() == event_type_names::kTouchstart)
    type_ = WebInputEvent::Type::kMouseDown;
  else if (event.type() == event_type_names::kTouchmove)
    type_ = WebInputEvent::Type::kMouseMove;
  else if (event.type() == event_type_names::kTouchend)
    type_ = WebInputEvent::Type::kMouseUp;
  else
    return;

  time_stamp_ = event.PlatformTimeStamp();
  modifiers_ = event.GetModifiers();
  frame_scale_ = 1;
  frame_translate_ = gfx::Vector2dF();

  // The mouse event co-ordinates should be generated from the co-ordinates of
  // the touch point.
  gfx::PointF screen_point = touch->ScreenLocation();
  SetPositionInScreen(screen_point.x(), screen_point.y());

  button = WebMouseEvent::Button::kLeft;
  modifiers_ |= WebInputEvent::kLeftButtonDown;
  click_count = (type_ == WebInputEvent::Type::kMouseDown ||
                 type_ == WebInputEvent::Type::kMouseUp);

  gfx::PointF local_point = layout_object->AbsoluteToLocalPoint(
      gfx::PointF(touch->AbsoluteLocation()));
  SetPositionInWidget(local_point);

  pointer_type = WebPointerProperties::PointerType::kTouch;
}

WebKeyboardEventBuilder::WebKeyboardEventBuilder(const KeyboardEvent& event) {
  if (const WebKeyboardEvent* web_event = event.KeyEvent()) {
    *static_cast<WebKeyboardEvent*>(this) = *web_event;
    return;
  }

  if (event.type() == event_type_names::kKeydown)
    type_ = WebInputEvent::Type::kKeyDown;
  else if (event.type() == event_type_names::kKeyup)
    type_ = WebInputEvent::Type::kKeyUp;
  else if (event.type() == event_type_names::kKeypress)
    type_ = WebInputEvent::Type::kChar;
  else
    return;  // Skip all other keyboard events.

  modifiers_ = event.GetModifiers();
  time_stamp_ = event.PlatformTimeStamp();
  windows_key_code = event.keyCode();
}

Vector<WebMouseEvent> TransformWebMouseEventVector(
    LocalFrameView* frame_view,
    const std::vector<std::unique_ptr<WebInputEvent>>& coalesced_events) {
  Vector<WebMouseEvent> result;
  for (const auto& event : coalesced_events) {
    DCHECK(WebInputEvent::IsMouseEventType(event->GetType()));
    result.push_back(TransformWebMouseEvent(
        frame_view, static_cast<const WebMouseEvent&>(*event)));
  }
  return result;
}

Vector<WebPointerEvent> TransformWebPointerEventVector(
    LocalFrameView* frame_view,
    const std::vector<std::unique_ptr<WebInputEvent>>& coalesced_events) {
  float scale = FrameScale(frame_view);
  gfx::Vector2dF translation = FrameTranslation(frame_view);
  Vector<WebPointerEvent> result;
  for (const auto& event : coalesced_events) {
    DCHECK(WebInputEvent::IsPointerEventType(event->GetType()));
    result.push_back(TransformWebPointerEvent(
        scale, translation, static_cast<const WebPointerEvent&>(*event)));
  }
  return result;
}

}  // namespace blink
