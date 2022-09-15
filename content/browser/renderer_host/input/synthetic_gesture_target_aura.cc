// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_aura.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

using blink::WebTouchEvent;
using blink::WebMouseWheelEvent;

namespace content {

namespace {

int WebEventButtonToUIEventButtonFlags(blink::WebMouseEvent::Button button) {
  if (button == blink::WebMouseEvent::Button::kLeft)
    return ui::EF_LEFT_MOUSE_BUTTON;
  if (button == blink::WebMouseEvent::Button::kMiddle)
    return ui::EF_MIDDLE_MOUSE_BUTTON;
  if (button == blink::WebMouseEvent::Button::kRight)
    return ui::EF_RIGHT_MOUSE_BUTTON;
  if (button == blink::WebMouseEvent::Button::kBack)
    return ui::EF_BACK_MOUSE_BUTTON;
  if (button == blink::WebMouseEvent::Button::kForward)
    return ui::EF_FORWARD_MOUSE_BUTTON;
  return 0;
}

}  // namespace

SyntheticGestureTargetAura::SyntheticGestureTargetAura(
    RenderWidgetHostImpl* host)
    : SyntheticGestureTargetBase(host) {
  device_scale_factor_ = host->GetDeviceScaleFactor();
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
  for (auto& event : events) {
    // Synthetic events from devtools debugger need to be dispatched explicitly
    // to the target window. Otherwise they will end up in the active tab
    // which might be different from the target.
    if (web_touch.GetModifiers() & blink::WebInputEvent::kFromDebugger) {
      window->delegate()->OnEvent(event.get());
    } else {
      event->ConvertLocationToTarget(window, host->window());
      ui::EventDispatchDetails details =
          event_injector_.Inject(host, event.get());
      if (details.dispatcher_destroyed)
        break;
    }
  }
}

void SyntheticGestureTargetAura::DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo&) {
  if (web_wheel.phase == blink::WebMouseWheelEvent::kPhaseEnded) {
    // Send the pending wheel end event immediately.
    GetView()->GetMouseWheelPhaseHandler()->DispatchPendingWheelEndEvent();
    return;
  }
  base::TimeTicks timestamp = web_wheel.TimeStamp();
  int modifiers = ui::WebEventModifiersToEventFlags(web_wheel.GetModifiers());
  if (web_wheel.delta_units == ui::ScrollGranularity::kScrollByPrecisePixel) {
    modifiers |= ui::EF_PRECISION_SCROLLING_DELTA;
  } else if (web_wheel.delta_units == ui::ScrollGranularity::kScrollByPage) {
    modifiers |= ui::EF_SCROLL_BY_PAGE;
  }

  float delta_x = web_wheel.delta_x + wheel_precision_x_;
  float delta_y = web_wheel.delta_y + wheel_precision_y_;
  ui::MouseWheelEvent wheel_event(
      gfx::Vector2d(delta_x, delta_y), web_wheel.PositionInWidget(),
      web_wheel.PositionInWidget(), timestamp, modifiers, ui::EF_NONE);
  wheel_precision_x_ = delta_x - wheel_event.x_offset();
  wheel_precision_y_ = delta_y - wheel_event.y_offset();

  aura::Window* window = GetWindow();
  // Synthetic events from devtools debugger need to be dispatched explicitly
  // to the target window. Otherwise they will end up in the active tab
  // which might be different from the target.
  if (web_wheel.GetModifiers() & blink::WebInputEvent::kFromDebugger) {
    window->delegate()->OnEvent(&wheel_event);
  } else {
    wheel_event.ConvertLocationToTarget(window, window->GetRootWindow());
    ui::EventDispatchDetails details =
        event_injector_.Inject(window->GetHost(), &wheel_event);
    if (details.dispatcher_destroyed)
      return;
  }
}

void SyntheticGestureTargetAura::DispatchWebGestureEventToPlatform(
    const blink::WebGestureEvent& web_gesture,
    const ui::LatencyInfo& latency_info) {
  DCHECK(blink::WebInputEvent::IsPinchGestureEventType(web_gesture.GetType()) ||
         blink::WebInputEvent::IsFlingGestureEventType(web_gesture.GetType()));
  ui::EventType event_type = web_gesture.GetTypeAsUiEventType();
  int flags = ui::WebEventModifiersToEventFlags(web_gesture.GetModifiers());
  aura::Window* window = GetWindow();

  if (blink::WebInputEvent::IsPinchGestureEventType(web_gesture.GetType())) {
    ui::GestureEventDetails pinch_details(event_type);
    pinch_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
    if (event_type == ui::ET_GESTURE_PINCH_UPDATE)
      pinch_details.set_scale(web_gesture.data.pinch_update.scale);

    ui::GestureEvent pinch_event(web_gesture.PositionInWidget().x(),
                                 web_gesture.PositionInWidget().y(), flags,
                                 web_gesture.TimeStamp(), pinch_details);

    // Synthetic events from devtools debugger need to be dispatched explicitly
    // to the target window. Otherwise they will end up in the active tab
    // which might be different from the target.
    if (web_gesture.GetModifiers() & blink::WebInputEvent::kFromDebugger) {
      window->delegate()->OnEvent(&pinch_event);
    } else {
      pinch_event.ConvertLocationToTarget(window, window->GetRootWindow());
      event_injector_.Inject(window->GetHost(), &pinch_event);
    }
    return;
  }

  ui::EventMomentumPhase momentum_phase =
      web_gesture.GetType() == blink::WebInputEvent::Type::kGestureFlingStart
          ? ui::EventMomentumPhase::BEGAN
          : ui::EventMomentumPhase::END;
  ui::ScrollEvent scroll_event(event_type, web_gesture.PositionInWidget(),
                               web_gesture.PositionInWidget(),
                               web_gesture.TimeStamp(), flags,
                               web_gesture.data.fling_start.velocity_x,
                               web_gesture.data.fling_start.velocity_y, 0, 0, 2,
                               momentum_phase, ui::ScrollEventPhase::kNone);
  // Synthetic events from devtools debugger need to be dispatched explicitly
  // to the target window. Otherwise they will end up in the active tab
  // which might be different from the target.
  if (web_gesture.GetModifiers() & blink::WebInputEvent::kFromDebugger) {
    window->delegate()->OnEvent(&scroll_event);
  } else {
    scroll_event.ConvertLocationToTarget(window, window->GetRootWindow());
    event_injector_.Inject(window->GetHost(), &scroll_event);
  }
}

void SyntheticGestureTargetAura::DispatchWebMouseEventToPlatform(
    const blink::WebMouseEvent& web_mouse_event,
    const ui::LatencyInfo& latency_info) {
  ui::EventType event_type = web_mouse_event.GetTypeAsUiEventType();
  int flags = ui::WebEventModifiersToEventFlags(web_mouse_event.GetModifiers());
  ui::PointerDetails pointer_details(
      ui::WebPointerTypeToEventPointerType(web_mouse_event.pointer_type),
      web_mouse_event.id, 0, 0, web_mouse_event.force, web_mouse_event.twist,
      web_mouse_event.tilt_x, web_mouse_event.tilt_y,
      web_mouse_event.tangential_pressure);
  int changed_button_flags = 0;
  if (event_type == ui::ET_MOUSE_PRESSED ||
      event_type == ui::ET_MOUSE_RELEASED) {
    changed_button_flags =
        WebEventButtonToUIEventButtonFlags(web_mouse_event.button);
  }
  ui::MouseEvent mouse_event(event_type, web_mouse_event.PositionInWidget(),
                             web_mouse_event.PositionInWidget(),
                             web_mouse_event.TimeStamp(), flags,
                             changed_button_flags, pointer_details);

  aura::Window* window = GetWindow();
  mouse_event.SetClickCount(web_mouse_event.click_count);
  // Synthetic events from devtools debugger need to be dispatched explicitly
  // to the target window. Otherwise they will end up in the active tab
  // which might be different from the target.
  if (web_mouse_event.GetModifiers() & blink::WebInputEvent::kFromDebugger) {
    window->delegate()->OnEvent(&mouse_event);
  } else {
    mouse_event.ConvertLocationToTarget(window, window->GetRootWindow());
    ui::EventDispatchDetails details =
        event_injector_.Inject(window->GetHost(), &mouse_event);
    if (details.dispatcher_destroyed)
      return;
  }
}

content::mojom::GestureSourceType
SyntheticGestureTargetAura::GetDefaultSyntheticGestureSourceType() const {
  return content::mojom::GestureSourceType::kMouseInput;
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

RenderWidgetHostViewAura* SyntheticGestureTargetAura::GetView() const {
  auto* view =
      static_cast<RenderWidgetHostViewAura*>(render_widget_host()->GetView());
  DCHECK(view);
  return view;
}

aura::Window* SyntheticGestureTargetAura::GetWindow() const {
  aura::Window* window = GetView()->GetNativeView();
  DCHECK(window);
  return window;
}

}  // namespace content
