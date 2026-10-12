// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INPUT_VISUALIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INPUT_VISUALIZER_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

class GraphicsContext;
class LocalFrame;
class WebInputEvent;

// InputVisualizer renders lightweight visual ripple animations for pointer
// down events on top of the web frame using a FrameOverlay, without
// modifying DOM, styles, or interfering with input events.
class CORE_EXPORT InputVisualizer final
    : public GarbageCollected<InputVisualizer> {
 public:
  static constexpr wtf_size_t kMaxConcurrentAnimations = 15;

  explicit InputVisualizer(LocalFrame& frame);
  ~InputVisualizer();

  void Trace(Visitor* visitor) const;

  void SetClicksEnabled(bool enabled);
  bool IsClicksEnabled() const { return clicks_enabled_; }

  void HandleInputEvent(const WebInputEvent& event);

  void UpdatePrePaint();
  void Paint(GraphicsContext& context);
  void Shutdown();

  void ServiceScriptedAnimations(base::TimeTicks monotonic_frame_begin_time);
  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& context,
                         const gfx::Size& view_size) const;
  void Invalidate();

  wtf_size_t ActiveClicksCountForTesting() const {
    return active_clicks_.size();
  }

 private:
  struct ClickPoint {
    gfx::PointF position;
    base::TimeTicks start_time;
  };

  void OnPointerDown(const gfx::PointF& position_in_widget,
                     base::TimeTicks timestamp);
  void EnsureFrameOverlay();
  void DestroyFrameOverlay();

  Member<LocalFrame> frame_;
  Member<FrameOverlay> frame_overlay_;
  Vector<ClickPoint> active_clicks_;
  base::TimeTicks last_frame_time_;
  bool clicks_enabled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INPUT_VISUALIZER_H_
