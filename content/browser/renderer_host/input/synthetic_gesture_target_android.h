// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/public/browser/android/motion_event_action.h"

namespace ui {
class LatencyInfo;
class ViewAndroid;
}  // namespace ui

namespace content {

// Owned by |SyntheticGestureController|. Keeps a strong pointer to Java object,
// which get destroyed together with the controller.
class SyntheticGestureTargetAndroid : public SyntheticGestureTargetBase {
 public:
  SyntheticGestureTargetAndroid(RenderWidgetHostImpl* host,
                                ui::ViewAndroid* view);
  ~SyntheticGestureTargetAndroid() override;

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
  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override;
  float GetTouchSlopInDips() const override;
  float GetMinScalingSpanInDips() const override;

 private:
  void TouchSetPointer(int index, float x, float y, int id);
  void TouchSetScrollDeltas(float x, float y, float dx, float dy);
  void TouchInject(MotionEventAction action,
                   int pointer_count,
                   base::TimeTicks time);

  RenderWidgetHostViewAndroid* GetView() const;

  ui::ViewAndroid* const view_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureTargetAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_ANDROID_H_
