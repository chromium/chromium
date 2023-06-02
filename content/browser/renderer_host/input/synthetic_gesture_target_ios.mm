// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/input/synthetic_gesture_target_ios.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

SyntheticGestureTargetIOS::SyntheticGestureTargetIOS(RenderWidgetHostImpl* host)
    : SyntheticGestureTargetBase(host) {}

void SyntheticGestureTargetIOS::DispatchWebGestureEventToPlatform(
    const WebGestureEvent& web_gesture,
    const ui::LatencyInfo& latency_info) {
  GetView()->InjectGestureEvent(web_gesture, latency_info);
}

void SyntheticGestureTargetIOS::DispatchWebTouchEventToPlatform(
    const blink::WebTouchEvent& web_touch,
    const ui::LatencyInfo& latency_info) {
  GetView()->InjectTouchEvent(web_touch, latency_info);
}

void SyntheticGestureTargetIOS::DispatchWebMouseWheelEventToPlatform(
    const blink::WebMouseWheelEvent& web_wheel,
    const ui::LatencyInfo& latency_info) {
  GetView()->InjectMouseWheelEvent(web_wheel, latency_info);
}

void SyntheticGestureTargetIOS::DispatchWebMouseEventToPlatform(
    const blink::WebMouseEvent& web_mouse,
    const ui::LatencyInfo& latency_info) {
  GetView()->InjectMouseEvent(web_mouse, latency_info);
}

content::mojom::GestureSourceType
SyntheticGestureTargetIOS::GetDefaultSyntheticGestureSourceType() const {
  return content::mojom::GestureSourceType::kTouchInput;
}

float SyntheticGestureTargetIOS::GetTouchSlopInDips() const {
  return ui::GestureConfiguration::GetInstance()
      ->max_touch_move_in_pixels_for_click();
}

float SyntheticGestureTargetIOS::GetSpanSlopInDips() const {
  return ui::GestureConfiguration::GetInstance()->span_slop();
}

float SyntheticGestureTargetIOS::GetMinScalingSpanInDips() const {
  return ui::GestureConfiguration::GetInstance()->min_scaling_span_in_pixels();
}

RenderWidgetHostViewIOS* SyntheticGestureTargetIOS::GetView() const {
  auto* view =
      static_cast<RenderWidgetHostViewIOS*>(render_widget_host()->GetView());
  DCHECK(view);
  return view;
}

}  // namespace content
