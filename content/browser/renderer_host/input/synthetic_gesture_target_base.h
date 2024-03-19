// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_BASE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_BASE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
class LatencyInfo;
}

namespace blink {
class WebTouchEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebGestureEvent;
}

namespace content {

class RenderWidgetHostImpl;

class SyntheticGestureTargetBase : public SyntheticGestureTarget {
 public:
  explicit SyntheticGestureTargetBase(RenderWidgetHostImpl* host);

  SyntheticGestureTargetBase(const SyntheticGestureTargetBase&) = delete;
  SyntheticGestureTargetBase& operator=(const SyntheticGestureTargetBase&) =
      delete;

  ~SyntheticGestureTargetBase() override;

  virtual void DispatchWebTouchEventToPlatform(
      const blink::WebTouchEvent& web_touch,
      const ui::LatencyInfo& latency_info) = 0;

  virtual void DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo& latency_info) = 0;

  virtual void DispatchWebGestureEventToPlatform(
      const blink::WebGestureEvent& web_gesture,
      const ui::LatencyInfo& latency_info) = 0;

  virtual void DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) = 0;

  // SyntheticGestureTarget:
  void DispatchInputEventToPlatform(const blink::WebInputEvent& event) override;
  void GetVSyncParameters(base::TimeTicks& timebase,
                          base::TimeDelta& interval) const override;

  base::TimeDelta PointerAssumedStoppedTime() const override;

  float GetSpanSlopInDips() const override;

  int GetMouseWheelMinimumGranularity() const override;

  void WaitForTargetAck(SyntheticGestureParams::GestureType type,
                        content::mojom::GestureSourceType source,
                        base::OnceClosure callback) const override;

 protected:
  RenderWidgetHostImpl* render_widget_host() const { return host_; }

 private:
  bool PointIsWithinContents(gfx::PointF point) const;

  raw_ptr<RenderWidgetHostImpl> host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_BASE_H_
