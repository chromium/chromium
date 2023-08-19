// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_touch_event.h"

#include <bitset>

namespace blink {

namespace {

const int kInvalidTouchIndex = -1;

int GetIndexOfTouchID(const WebTouchEvent& event, int id) {
  for (unsigned i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].id == id)
      return i;
  }
  return kInvalidTouchIndex;
}

}  // namespace

std::unique_ptr<WebInputEvent> WebTouchEvent::Clone() const {
  return std::make_unique<WebTouchEvent>(*this);
}

bool WebTouchEvent::CanCoalesce(const WebInputEvent& event) const {
  if (!IsTouchEventType(event.GetType()))
    return false;
  const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);

  if (GetType() != touch_event.GetType() ||
      GetType() != WebInputEvent::Type::kTouchMove ||
      GetModifiers() != touch_event.GetModifiers() ||
      touches_length != touch_event.touches_length ||
      touches_length > kTouchesLengthCap)
    return false;

  static_assert(WebTouchEvent::kTouchesLengthCap <= sizeof(int32_t) * 8U,
                "suboptimal kTouchesLengthCap size");
  // Ensure that we have a 1-to-1 mapping of pointer ids between touches.
  std::bitset<WebTouchEvent::kTouchesLengthCap> unmatched_event_touches(
      (1 << touches_length) - 1);
  for (unsigned i = 0; i < touch_event.touches_length; ++i) {
    int event_touch_index = GetIndexOfTouchID(*this, touch_event.touches[i].id);
    if (event_touch_index == kInvalidTouchIndex)
      return false;
    if (!unmatched_event_touches[event_touch_index])
      return false;
    if (touches[event_touch_index].pointer_type !=
        touch_event.touches[i].pointer_type)
      return false;
    unmatched_event_touches[event_touch_index] = false;
  }
  return unmatched_event_touches.none();
}

void WebTouchEvent::Coalesce(const WebInputEvent& event) {
  DCHECK(CanCoalesce(event));
  const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);

  // The WebTouchPoints include absolute position information. So it is
  // sufficient to simply replace the previous event with the new event->
  // However, it is necessary to make sure that all the points have the
  // correct state, i.e. the touch-points that moved in the last event, but
  // didn't change in the current event, will have Stationary state. It is
  // necessary to change them back to Moved state.
  WebTouchEvent old_event = *this;
  *this = touch_event;
  for (unsigned i = 0; i < touches_length; ++i) {
    int i_old = GetIndexOfTouchID(old_event, touches[i].id);
    if (old_event.touches[i_old].state == WebTouchPoint::State::kStateMoved) {
      touches[i].state = WebTouchPoint::State::kStateMoved;
      touches[i].movement_x += old_event.touches[i_old].movement_x;
      touches[i].movement_y += old_event.touches[i_old].movement_y;
    }
  }
  moved_beyond_slop_region |= old_event.moved_beyond_slop_region;
  dispatch_type =
      MergeDispatchTypes(old_event.dispatch_type, touch_event.dispatch_type);
  unique_touch_event_id = old_event.unique_touch_event_id;
}

WebTouchEvent WebTouchEvent::FlattenTransform() const {
  WebTouchEvent transformed_event = *this;
  for (unsigned i = 0; i < touches_length; ++i) {
    transformed_event.touches[i] = TouchPointInRootFrame(i);
  }
  transformed_event.frame_translate_ = gfx::Vector2dF();
  transformed_event.frame_scale_ = 1;

  return transformed_event;
}

WebTouchPoint WebTouchEvent::TouchPointInRootFrame(unsigned point) const {
  DCHECK_LT(point, touches_length);
  if (point >= touches_length)
    return WebTouchPoint();

  WebTouchPoint transformed_point = touches[point];
  transformed_point.radius_x /= frame_scale_;
  transformed_point.radius_y /= frame_scale_;
  transformed_point.SetPositionInWidget(
      gfx::ScalePoint(transformed_point.PositionInWidget(), 1 / frame_scale_) +
      frame_translate_);
  return transformed_point;
}

bool WebTouchEvent::IsTouchSequenceStart() const {
  DCHECK(touches_length ||
         GetType() == WebInputEvent::Type::kTouchScrollStarted);
  if (GetType() != WebInputEvent::Type::kTouchStart) {
    return false;
  }
  for (size_t i = 0; i < touches_length; ++i) {
    if (touches[i].state != WebTouchPoint::State::kStatePressed) {
      return false;
    }
  }
  return true;
}

bool WebTouchEvent::IsTouchSequenceEnd() const {
  if (GetType() != WebInputEvent::Type::kTouchEnd &&
      GetType() != WebInputEvent::Type::kTouchCancel) {
    return false;
  }
  if (!touches_length) {
    return true;
  }
  for (size_t i = 0; i < touches_length; ++i) {
    if (touches[i].state != WebTouchPoint::State::kStateReleased &&
        touches[i].state != WebTouchPoint::State::kStateCancelled) {
      return false;
    }
  }
  return true;
}

}  // namespace blink
