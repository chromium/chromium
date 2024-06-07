// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/motion_event_web.h"

#include <numbers>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "components/input/web_touch_event_traits.h"
#include "ui/events/blink/blink_event_util.h"

using blink::WebInputEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using input::WebTouchEventTraits;

namespace content {
namespace {

ui::MotionEvent::Action GetActionFrom(const WebTouchEvent& event) {
  DCHECK(event.touches_length);
  switch (event.GetType()) {
    case WebInputEvent::Type::kTouchStart:
      if (WebTouchEventTraits::AllTouchPointsHaveState(
              event, WebTouchPoint::State::kStatePressed))
        return ui::MotionEvent::Action::DOWN;
      else
        return ui::MotionEvent::Action::POINTER_DOWN;
    case WebInputEvent::Type::kTouchEnd:
      if (WebTouchEventTraits::AllTouchPointsHaveState(
              event, WebTouchPoint::State::kStateReleased))
        return ui::MotionEvent::Action::UP;
      else
        return ui::MotionEvent::Action::POINTER_UP;
    case WebInputEvent::Type::kTouchCancel:
      DCHECK(WebTouchEventTraits::AllTouchPointsHaveState(
          event, WebTouchPoint::State::kStateCancelled));
      return ui::MotionEvent::Action::CANCEL;
    case WebInputEvent::Type::kTouchMove:
      return ui::MotionEvent::Action::MOVE;
    default:
      break;
  };
  NOTREACHED_IN_MIGRATION()
      << "Unable to derive a valid MotionEvent::Action from the WebTouchEvent.";
  return ui::MotionEvent::Action::CANCEL;
}

int GetActionIndexFrom(const WebTouchEvent& event) {
  for (size_t i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].state != WebTouchPoint::State::kStateUndefined &&
        event.touches[i].state != WebTouchPoint::State::kStateStationary)
      return i;
  }
  return -1;
}

}  // namespace

MotionEventWeb::MotionEventWeb(const WebTouchEvent& event)
    : event_(event),
      cached_action_(GetActionFrom(event)),
      cached_action_index_(GetActionIndexFrom(event)),
      unique_event_id_(event.unique_touch_event_id) {
  DCHECK_GT(GetPointerCount(), 0U);
}

MotionEventWeb::~MotionEventWeb() {}

uint32_t MotionEventWeb::GetUniqueEventId() const {
  return unique_event_id_;
}

MotionEventWeb::Action MotionEventWeb::GetAction() const {
  return cached_action_;
}

int MotionEventWeb::GetActionIndex() const {
  DCHECK(cached_action_ == Action::POINTER_UP ||
         cached_action_ == Action::POINTER_DOWN)
      << "Invalid action for GetActionIndex(): " << cached_action_;
  DCHECK_GE(cached_action_index_, 0);
  DCHECK_LT(cached_action_index_, static_cast<int>(event_.touches_length));
  return cached_action_index_;
}

size_t MotionEventWeb::GetPointerCount() const {
  return event_.touches_length;
}

int MotionEventWeb::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].id;
}

float MotionEventWeb::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].PositionInWidget().x();
}

float MotionEventWeb::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].PositionInWidget().y();
}

float MotionEventWeb::GetRawX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].PositionInScreen().x();
}

float MotionEventWeb::GetRawY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return event_.touches[pointer_index].PositionInScreen().y();
}

float MotionEventWeb::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return 2.f * std::max(event_.touches[pointer_index].radius_x,
                        event_.touches[pointer_index].radius_y);
}

float MotionEventWeb::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return 2.f * std::min(event_.touches[pointer_index].radius_x,
                        event_.touches[pointer_index].radius_y);
}

float MotionEventWeb::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  float orientation_rad =
      base::DegToRad(event_.touches[pointer_index].rotation_angle);
  DCHECK(0 <= orientation_rad &&
         orientation_rad <= std::numbers::pi_v<float> / 2)
      << "Unexpected touch rotation angle";

  if (GetToolType(pointer_index) == ToolType::STYLUS) {
    const WebPointerProperties& pointer = event_.touches[pointer_index];

    if (pointer.tilt_y <= 0 && pointer.tilt_x < 0) {
      // Stylus is tilted to the left away from the user or straight
      // to the left thus the orientation should be within [pi/2,pi).
      orientation_rad += std::numbers::pi_v<float> / 2;
    } else if (pointer.tilt_y < 0 && pointer.tilt_x >= 0) {
      // Stylus is tilted to the right away from the user or straight away
      // from the user thus the orientation should be within [-pi,-pi/2).
      orientation_rad -= std::numbers::pi_v<float>;
    } else if (pointer.tilt_y >= 0 && pointer.tilt_x > 0) {
      // Stylus is tilted to the right towards the user or straight
      // to the right thus the orientation should be within [-pi/2,0).
      orientation_rad -= std::numbers::pi_v<float> / 2;
    }
  } else if (event_.touches[pointer_index].radius_x >
             event_.touches[pointer_index].radius_y) {
    // The case radiusX == radiusY is omitted from here on purpose: for circles,
    // we want to pass the angle (which could be any value in such cases but
    // always seems to be set to zero) unchanged.
    orientation_rad -= std::numbers::pi_v<float> / 2;
  }

  return orientation_rad;
}

float MotionEventWeb::GetPressure(size_t pointer_index) const {
  return 0.f;
}

float MotionEventWeb::GetTiltX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  return event_.touches[pointer_index].tilt_x;
}

float MotionEventWeb::GetTiltY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  return event_.touches[pointer_index].tilt_y;
}

float MotionEventWeb::GetTwist(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  return event_.touches[pointer_index].twist;
}

float MotionEventWeb::GetTangentialPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  return event_.touches[pointer_index].tangential_pressure;
}

base::TimeTicks MotionEventWeb::GetEventTime() const {
  return event_.TimeStamp();
}

ui::MotionEvent::ToolType MotionEventWeb::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());

  const WebPointerProperties& pointer = event_.touches[pointer_index];

  switch (pointer.pointer_type) {
    case WebPointerProperties::PointerType::kUnknown:
      return ToolType::UNKNOWN;
    case WebPointerProperties::PointerType::kMouse:
      return ToolType::MOUSE;
    case WebPointerProperties::PointerType::kPen:
      return ToolType::STYLUS;
    case WebPointerProperties::PointerType::kEraser:
      return ToolType::ERASER;
    case WebPointerProperties::PointerType::kTouch:
      return ToolType::FINGER;
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected pointerType";
  return ToolType::UNKNOWN;
}

int MotionEventWeb::GetButtonState() const {
  return 0;
}

int MotionEventWeb::GetFlags() const {
  return ui::WebEventModifiersToEventFlags(event_.GetModifiers());
}

}  // namespace content
