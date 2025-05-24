// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"

#include "build/build_config.h"

namespace blink {

namespace {

float GetUnacceleratedDelta(float accelerated_delta, float acceleration_ratio) {
  return accelerated_delta * acceleration_ratio;
}

float GetAccelerationRatio(float accelerated_delta, float unaccelerated_delta) {
  if (unaccelerated_delta == 0.f || accelerated_delta == 0.f)
    return 1.f;
  return unaccelerated_delta / accelerated_delta;
}

}  // namespace

std::unique_ptr<WebInputEvent> WebMouseWheelEvent::Clone() const {
  return std::make_unique<WebMouseWheelEvent>(*this);
}

bool WebMouseWheelEvent::CanCoalesce(const WebInputEvent& event) const {
  if (event.GetType() != WebInputEvent::Type::kMouseWheel)
    return false;
  const WebMouseWheelEvent& mouse_wheel_event =
      static_cast<const WebMouseWheelEvent&>(event);

  return GetModifiers() == mouse_wheel_event.GetModifiers() &&
         delta_units == mouse_wheel_event.delta_units &&
         HaveConsistentPhase(mouse_wheel_event);
}

bool WebMouseWheelEvent::HaveConsistentPhase(
    const WebMouseWheelEvent& event) const {
  if (has_synthetic_phase != event.has_synthetic_phase)
    return false;

  if (phase == event.phase && momentum_phase == event.momentum_phase) {
    return true;
  }

  if (has_synthetic_phase) {
    // It is alright to coalesce a wheel event with synthetic phaseChanged to
    // its previous one with synthetic phaseBegan.
    return (phase == WebMouseWheelEvent::kPhaseBegan &&
            event.phase == WebMouseWheelEvent::kPhaseChanged);
  }
  return false;
}

void WebMouseWheelEvent::Coalesce(const WebInputEvent& event) {
  DCHECK(CanCoalesce(event));
  const WebMouseWheelEvent& mouse_wheel_event =
      static_cast<const WebMouseWheelEvent&>(event);
  float unaccelerated_x =
      GetUnacceleratedDelta(delta_x, acceleration_ratio_x) +
      GetUnacceleratedDelta(mouse_wheel_event.delta_x,
                            mouse_wheel_event.acceleration_ratio_x);
  float unaccelerated_y =
      GetUnacceleratedDelta(delta_y, acceleration_ratio_y) +
      GetUnacceleratedDelta(mouse_wheel_event.delta_y,
                            mouse_wheel_event.acceleration_ratio_y);
  float old_deltaX = delta_x;
  float old_deltaY = delta_y;
  float old_wheelTicksX = wheel_ticks_x;
  float old_wheelTicksY = wheel_ticks_y;
  float old_movementX = movement_x;
  float old_movementY = movement_y;
  WebMouseWheelEvent::Phase old_phase = phase;
  WebInputEvent::DispatchType old_dispatch_type = dispatch_type;
  *this = mouse_wheel_event;
  delta_x += old_deltaX;
  delta_y += old_deltaY;
  wheel_ticks_x += old_wheelTicksX;
  wheel_ticks_y += old_wheelTicksY;
  movement_x += old_movementX;
  movement_y += old_movementY;
  acceleration_ratio_x = GetAccelerationRatio(delta_x, unaccelerated_x);
  acceleration_ratio_y = GetAccelerationRatio(delta_y, unaccelerated_y);
  dispatch_type =
      MergeDispatchTypes(old_dispatch_type, mouse_wheel_event.dispatch_type);
  if (mouse_wheel_event.has_synthetic_phase &&
      mouse_wheel_event.phase != old_phase) {
    // Coalesce  a wheel event with synthetic phase changed to a wheel event
    // with synthetic phase began.
    DCHECK_EQ(WebMouseWheelEvent::kPhaseChanged, mouse_wheel_event.phase);
    DCHECK_EQ(WebMouseWheelEvent::kPhaseBegan, old_phase);
    phase = WebMouseWheelEvent::kPhaseBegan;
  }
}

float WebMouseWheelEvent::DeltaXInRootFrame() const {
  return delta_x / frame_scale_;
}

float WebMouseWheelEvent::DeltaYInRootFrame() const {
  return delta_y / frame_scale_;
}

WebMouseWheelEvent WebMouseWheelEvent::FlattenTransform() const {
  WebMouseWheelEvent result = *this;
  result.delta_x /= result.frame_scale_;
  result.delta_y /= result.frame_scale_;
  result.FlattenTransformSelf();
  return result;
}

// static
WebMouseWheelEvent::EventAction
WebMouseWheelEvent::GetPlatformSpecificDefaultEventAction(
    const WebMouseWheelEvent& event) {
#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)
  // Scroll events generated from the mouse wheel when the control key is held
  // don't trigger scrolling. Instead, they may cause zooming.
  if (event.delta_units != ui::ScrollGranularity::kScrollByPrecisePixel &&
      (event.GetModifiers() & WebInputEvent::kControlKey)) {
    return blink::WebMouseWheelEvent::EventAction::kPageZoom;
  }

  if (event.delta_x == 0 && (event.GetModifiers() & WebInputEvent::kShiftKey))
    return blink::WebMouseWheelEvent::EventAction::kScrollHorizontal;
#endif
  if (event.rails_mode == WebInputEvent::kRailsModeHorizontal ||
      (event.delta_x != 0 && event.delta_y == 0)) {
    return blink::WebMouseWheelEvent::EventAction::kScrollHorizontal;
  }

  if (event.rails_mode == WebInputEvent::kRailsModeVertical ||
      (event.delta_x == 0 && event.delta_y != 0)) {
    return blink::WebMouseWheelEvent::EventAction::kScrollVertical;
  }

  return blink::WebMouseWheelEvent::EventAction::kScroll;
}

}  // namespace blink
