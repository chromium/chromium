// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_mouse_event.h"

#include "third_party/blink/public/platform/web_gesture_event.h"

namespace blink {

WebMouseEvent::WebMouseEvent(WebInputEvent::Type type,
                             const WebGestureEvent& gesture_event,
                             Button button_param,
                             int click_count_param,
                             int modifiers,
                             base::TimeTicks time_stamp,
                             PointerId id_param)
    : WebInputEvent(sizeof(WebMouseEvent), type, modifiers, time_stamp),
      WebPointerProperties(id_param,
                           WebPointerProperties::PointerType::kMouse,
                           button_param),
      click_count(click_count_param) {
  DCHECK_GE(type, kMouseTypeFirst);
  DCHECK_LE(type, kMouseTypeLast);
  SetPositionInWidget(gesture_event.PositionInWidget());
  SetPositionInScreen(gesture_event.PositionInScreen());
  SetFrameScale(gesture_event.FrameScale());
  SetFrameTranslate(gesture_event.FrameTranslate());
  SetMenuSourceType(gesture_event.GetType());
}

WebFloatPoint WebMouseEvent::PositionInRootFrame() const {
  return WebFloatPoint(
      (position_in_widget_.x / frame_scale_) + frame_translate_.x,
      (position_in_widget_.y / frame_scale_) + frame_translate_.y);
}

WebMouseEvent WebMouseEvent::FlattenTransform() const {
  WebMouseEvent result = *this;
  result.FlattenTransformSelf();
  return result;
}

void WebMouseEvent::FlattenTransformSelf() {
  position_in_widget_ = PositionInRootFrame();
  frame_translate_.x = 0;
  frame_translate_.y = 0;
  frame_scale_ = 1;
}

void WebMouseEvent::SetMenuSourceType(WebInputEvent::Type type) {
  switch (type) {
    case kGestureTapDown:
    case kGestureTap:
    case kGestureDoubleTap:
      menu_source_type = kMenuSourceTouch;
      break;
    case kGestureLongPress:
      menu_source_type = kMenuSourceLongPress;
      break;
    case kGestureLongTap:
      menu_source_type = kMenuSourceLongTap;
      break;
    default:
      menu_source_type = kMenuSourceNone;
  }
}

}  // namespace blink
