// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_gesture_event.h"

namespace blink {

float WebGestureEvent::DeltaXInRootFrame() const {
  if (type_ == WebInputEvent::kGestureScrollBegin)
    return data.scroll_begin.delta_x_hint / frame_scale_;
  DCHECK(type_ == WebInputEvent::kGestureScrollUpdate);
  return data.scroll_update.delta_x / frame_scale_;
}

float WebGestureEvent::DeltaYInRootFrame() const {
  if (type_ == WebInputEvent::kGestureScrollBegin)
    return data.scroll_begin.delta_y_hint / frame_scale_;
  DCHECK(type_ == WebInputEvent::kGestureScrollUpdate);
  return data.scroll_update.delta_y / frame_scale_;
}

ui::input_types::ScrollGranularity WebGestureEvent::DeltaUnits() const {
  if (type_ == WebInputEvent::kGestureScrollBegin)
    return data.scroll_begin.delta_hint_units;
  if (type_ == WebInputEvent::kGestureScrollUpdate)
    return data.scroll_update.delta_units;
  DCHECK(type_ == WebInputEvent::kGestureScrollEnd);
  return data.scroll_end.delta_units;
}

WebGestureEvent::InertialPhaseState WebGestureEvent::InertialPhase() const {
  if (type_ == WebInputEvent::kGestureScrollBegin)
    return data.scroll_begin.inertial_phase;
  if (type_ == WebInputEvent::kGestureScrollUpdate)
    return data.scroll_update.inertial_phase;
  DCHECK(type_ == WebInputEvent::kGestureScrollEnd);
  return data.scroll_end.inertial_phase;
}

bool WebGestureEvent::Synthetic() const {
  if (type_ == WebInputEvent::kGestureScrollBegin)
    return data.scroll_begin.synthetic;
  DCHECK(type_ == WebInputEvent::kGestureScrollEnd);
  return data.scroll_end.synthetic;
}

float WebGestureEvent::VelocityX() const {
  if (type_ == WebInputEvent::kGestureScrollUpdate)
    return data.scroll_update.velocity_x;
  DCHECK(type_ == WebInputEvent::kGestureFlingStart);
  return data.fling_start.velocity_x;
}

float WebGestureEvent::VelocityY() const {
  if (type_ == WebInputEvent::kGestureScrollUpdate)
    return data.scroll_update.velocity_y;
  DCHECK(type_ == WebInputEvent::kGestureFlingStart);
  return data.fling_start.velocity_y;
}

WebFloatSize WebGestureEvent::TapAreaInRootFrame() const {
  if (type_ == WebInputEvent::kGestureTwoFingerTap) {
    return WebFloatSize(data.two_finger_tap.first_finger_width / frame_scale_,
                        data.two_finger_tap.first_finger_height / frame_scale_);
  } else if (type_ == WebInputEvent::kGestureLongPress ||
             type_ == WebInputEvent::kGestureLongTap) {
    return WebFloatSize(data.long_press.width / frame_scale_,
                        data.long_press.height / frame_scale_);
  } else if (type_ == WebInputEvent::kGestureTap ||
             type_ == WebInputEvent::kGestureTapUnconfirmed ||
             type_ == WebInputEvent::kGestureDoubleTap) {
    return WebFloatSize(data.tap.width / frame_scale_,
                        data.tap.height / frame_scale_);
  } else if (type_ == WebInputEvent::kGestureTapDown) {
    return WebFloatSize(data.tap_down.width / frame_scale_,
                        data.tap_down.height / frame_scale_);
  } else if (type_ == WebInputEvent::kGestureShowPress) {
    return WebFloatSize(data.show_press.width / frame_scale_,
                        data.show_press.height / frame_scale_);
  }
  // This function is called for all gestures and determined if the tap
  // area is empty or not; so return an empty rect here.
  return WebFloatSize();
}

WebFloatPoint WebGestureEvent::PositionInRootFrame() const {
  return WebFloatPoint(
      (position_in_widget_.x / frame_scale_) + frame_translate_.x,
      (position_in_widget_.y / frame_scale_) + frame_translate_.y);
}

int WebGestureEvent::TapCount() const {
  DCHECK(type_ == WebInputEvent::kGestureTap);
  return data.tap.tap_count;
}

void WebGestureEvent::ApplyTouchAdjustment(WebFloatPoint root_frame_coords) {
  // Update the window-relative position of the event so that the node that
  // was ultimately hit is under this point (i.e. elementFromPoint for the
  // client co-ordinates in a 'click' event should yield the target). The
  // global position is intentionally left unmodified because it's intended to
  // reflect raw co-ordinates unrelated to any content.
  frame_translate_.x =
      root_frame_coords.x - (position_in_widget_.x / frame_scale_);
  frame_translate_.y =
      root_frame_coords.y - (position_in_widget_.y / frame_scale_);
}

void WebGestureEvent::FlattenTransform() {
  if (frame_scale_ != 1) {
    switch (type_) {
      case WebInputEvent::kGestureScrollBegin:
        data.scroll_begin.delta_x_hint /= frame_scale_;
        data.scroll_begin.delta_y_hint /= frame_scale_;
        break;
      case WebInputEvent::kGestureScrollUpdate:
        data.scroll_update.delta_x /= frame_scale_;
        data.scroll_update.delta_y /= frame_scale_;
        break;
      case WebInputEvent::kGestureTwoFingerTap:
        data.two_finger_tap.first_finger_width /= frame_scale_;
        data.two_finger_tap.first_finger_height /= frame_scale_;
        break;
      case WebInputEvent::kGestureLongPress:
      case WebInputEvent::kGestureLongTap:
        data.long_press.width /= frame_scale_;
        data.long_press.height /= frame_scale_;
        break;
      case WebInputEvent::kGestureTap:
      case WebInputEvent::kGestureTapUnconfirmed:
      case WebInputEvent::kGestureDoubleTap:
        data.tap.width /= frame_scale_;
        data.tap.height /= frame_scale_;
        break;
      case WebInputEvent::kGestureTapDown:
        data.tap_down.width /= frame_scale_;
        data.tap_down.height /= frame_scale_;
        break;
      case WebInputEvent::kGestureShowPress:
        data.show_press.width /= frame_scale_;
        data.show_press.height /= frame_scale_;
        break;
      default:
        break;
    }
  }

  SetPositionInWidget(PositionInRootFrame());
  frame_translate_.x = 0;
  frame_translate_.y = 0;
  frame_scale_ = 1;
}

}  // namespace blink
