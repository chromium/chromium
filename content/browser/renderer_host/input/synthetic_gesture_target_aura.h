// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_AURA_H_

#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "ui/aura/event_injector.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {

// SyntheticGestureTarget implementation for aura
class SyntheticGestureTargetAura : public SyntheticGestureTargetBase {
 public:
  explicit SyntheticGestureTargetAura(RenderWidgetHostImpl* host);

  SyntheticGestureTargetAura(const SyntheticGestureTargetAura&) = delete;
  SyntheticGestureTargetAura& operator=(const SyntheticGestureTargetAura&) =
      delete;

  // SyntheticGestureTargetBase:
  void DispatchWebTouchEventToPlatform(
      const blink::WebTouchEvent& web_touch,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebGestureEventToPlatform(
      const blink::WebGestureEvent& web_gesture,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) override;

  // SyntheticGestureTarget:
  content::mojom::GestureSourceType GetDefaultSyntheticGestureSourceType()
      const override;

  float GetTouchSlopInDips() const override;

  float GetSpanSlopInDips() const override;

  float GetMinScalingSpanInDips() const override;

 private:
  RenderWidgetHostViewAura* GetView() const;
  aura::Window* GetWindow() const;

  // Synthetic located event's location and touch event's radius are in DIP and
  // aura event dispatcher assumes input event is in device pixel and will apply
  // device scale factor to convert the input to DIP. So we need to use
  // device_scale_factor to convert the input event from DIP to device pixel
  // before dispatching it into platform.
  float device_scale_factor_;

  float wheel_precision_x_ = 0.f;
  float wheel_precision_y_ = 0.f;

  aura::EventInjector event_injector_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_AURA_H_
