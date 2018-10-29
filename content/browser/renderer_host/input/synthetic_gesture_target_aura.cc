// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_aura.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/event_injector.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

using blink::WebTouchEvent;
using blink::WebMouseWheelEvent;

namespace content {

SyntheticGestureTargetAura::SyntheticGestureTargetAura(
    RenderWidgetHostImpl* host)
    : SyntheticGestureTargetBase(host) {
  ScreenInfo screen_info;
  host->GetScreenInfo(&screen_info);
  device_scale_factor_ = screen_info.device_scale_factor;
}

void SyntheticGestureTargetAura::DispatchWebTouchEventToPlatform(
    const WebTouchEvent& web_touch,
    const ui::LatencyInfo& latency_info) {
  TouchEventWithLatencyInfo touch_with_latency(web_touch, latency_info);
  for (size_t i = 0; i < touch_with_latency.event.touches_length; i++) {
    touch_with_latency.event.touches[i].radius_x *= device_scale_factor_;
    touch_with_latency.event.touches[i].radius_y *= device_scale_factor_;
  }
  std::vector<std::unique_ptr<ui::TouchEvent>> events;
  bool conversion_success = MakeUITouchEventsFromWebTouchEvents(
      touch_with_latency, &events, LOCAL_COORDINATES);
  DCHECK(conversion_success);

  aura::Window* window = GetWindow();
  aura::WindowTreeHost* host = window->GetHost();
  aura::EventInjector injector;

  for (const auto& event : events) {
    event->ConvertLocationToTarget(window, host->window());
    // Apply the screen scale factor to the event location after it has been
    // transformed to the target.
    gfx::PointF device_location =
        gfx::ScalePoint(event->location_f(), device_scale_factor_);
    gfx::PointF device_root_location =
        gfx::ScalePoint(event->root_location_f(), device_scale_factor_);
    event->set_location_f(device_location);
    event->set_root_location_f(device_root_location);
    ui::EventDispatchDetails details = injector.Inject(host, event.get());
    if (details.dispatcher_destroyed)
      break;
  }
}

void SyntheticGestureTargetAura::DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo&) {
  if (web_wheel.phase == blink::WebMouseWheelEvent::kPhaseEnded) {
    DCHECK(
        !render_widget_host()->GetView()->IsRenderWidgetHostViewChildFrame() &&
        !render_widget_host()->GetView()->IsRenderWidgetHostViewGuest());
    // Send the pending wheel end event immediately.
    static_cast<RenderWidgetHostViewAura*>(render_widget_host()->GetView())
        ->event_handler()
        ->mouse_wheel_phase_handler()
        .DispatchPendingWheelEndEvent();
    return;
  }
  base::TimeTicks timestamp = web_wheel.TimeStamp();
  int modifiers = ui::EF_NONE;
  if (web_wheel.has_precise_scrolling_deltas)
    modifiers |= ui::EF_PRECISION_SCROLLING_DELTA;

  if (web_wheel.scroll_by_page)
    modifiers |= ui::EF_SCROLL_BY_PAGE;

  ui::MouseWheelEvent wheel_event(
      gfx::Vector2d(web_wheel.delta_x, web_wheel.delta_y), gfx::Point(),
      gfx::Point(), timestamp, modifiers, ui::EF_NONE);
  gfx::PointF location(web_wheel.PositionInWidget().x * device_scale_factor_,
                       web_wheel.PositionInWidget().y * device_scale_factor_);
  wheel_event.set_location_f(location);
  wheel_event.set_root_location_f(location);

  aura::Window* window = GetWindow();
  wheel_event.ConvertLocationToTarget(window, window->GetRootWindow());
  aura::EventInjector injector;
  ui::EventDispatchDetails details =
      injector.Inject(window->GetHost(), &wheel_event);
  if (details.dispatcher_destroyed)
    return;
}

void SyntheticGestureTargetAura::DispatchWebGestureEventToPlatform(
    const blink::WebGestureEvent& web_gesture,
    const ui::LatencyInfo& latency_info) {
  DCHECK(blink::WebInputEvent::IsPinchGestureEventType(web_gesture.GetType()) ||
         blink::WebInputEvent::IsFlingGestureEventType(web_gesture.GetType()));
  ui::EventType event_type = ui::WebEventTypeToEventType(web_gesture.GetType());
  int flags = ui::WebEventModifiersToEventFlags(web_gesture.GetModifiers());
  aura::Window* window = GetWindow();
  aura::EventInjector injector;

  if (blink::WebInputEvent::IsPinchGestureEventType(web_gesture.GetType())) {
    ui::GestureEventDetails pinch_details(event_type);
    pinch_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
    if (event_type == ui::ET_GESTURE_PINCH_UPDATE)
      pinch_details.set_scale(web_gesture.data.pinch_update.scale);

    ui::GestureEvent pinch_event(
        web_gesture.PositionInWidget().x * device_scale_factor_,
        web_gesture.PositionInWidget().y * device_scale_factor_, flags,
        ui::EventTimeForNow(), pinch_details);

    pinch_event.ConvertLocationToTarget(window, window->GetRootWindow());

    injector.Inject(window->GetHost(), &pinch_event);
    return;
  }

  ui::EventMomentumPhase momentum_phase =
      web_gesture.GetType() == blink::WebInputEvent::kGestureFlingStart
          ? ui::EventMomentumPhase::BEGAN
          : ui::EventMomentumPhase::END;
  gfx::PointF location(web_gesture.PositionInWidget().x * device_scale_factor_,
                       web_gesture.PositionInWidget().y * device_scale_factor_);
  ui::ScrollEvent scroll_event(event_type, gfx::Point(), ui::EventTimeForNow(),
                               flags, web_gesture.data.fling_start.velocity_x,
                               web_gesture.data.fling_start.velocity_y, 0, 0, 2,
                               momentum_phase, ui::ScrollEventPhase::kNone);
  scroll_event.set_location_f(location);
  scroll_event.set_root_location_f(location);
  scroll_event.ConvertLocationToTarget(window, window->GetRootWindow());
  injector.Inject(window->GetHost(), &scroll_event);
}

void SyntheticGestureTargetAura::DispatchWebMouseEventToPlatform(
    const blink::WebMouseEvent& web_mouse_event,
    const ui::LatencyInfo& latency_info) {
  ui::EventType event_type =
      ui::WebEventTypeToEventType(web_mouse_event.GetType());
  int flags = ui::WebEventModifiersToEventFlags(web_mouse_event.GetModifiers());
  ui::PointerDetails pointer_details(
      ui::WebPointerTypeToEventPointerType(web_mouse_event.pointer_type));
  ui::MouseEvent mouse_event(event_type, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), flags, flags,
                             pointer_details);
  gfx::PointF location(
      web_mouse_event.PositionInWidget().x * device_scale_factor_,
      web_mouse_event.PositionInWidget().y * device_scale_factor_);
  mouse_event.set_location_f(location);
  mouse_event.set_root_location_f(location);

  aura::Window* window = GetWindow();
  mouse_event.ConvertLocationToTarget(window, window->GetRootWindow());
  aura::EventInjector injector;
  ui::EventDispatchDetails details =
      injector.Inject(window->GetHost(), &mouse_event);
  if (details.dispatcher_destroyed)
    return;
}

SyntheticGestureParams::GestureSourceType
SyntheticGestureTargetAura::GetDefaultSyntheticGestureSourceType() const {
  return SyntheticGestureParams::TOUCH_INPUT;
}

float SyntheticGestureTargetAura::GetTouchSlopInDips() const {
  return ui::GestureConfiguration::GetInstance()
      ->max_touch_move_in_pixels_for_click();
}

float SyntheticGestureTargetAura::GetSpanSlopInDips() const {
  return ui::GestureConfiguration::GetInstance()->span_slop();
}

float SyntheticGestureTargetAura::GetMinScalingSpanInDips() const {
  return ui::GestureConfiguration::GetInstance()->min_scaling_span_in_pixels();
}

aura::Window* SyntheticGestureTargetAura::GetWindow() const {
  aura::Window* window = render_widget_host()->GetView()->GetNativeView();
  DCHECK(window);
  return window;
}

}  // namespace content
