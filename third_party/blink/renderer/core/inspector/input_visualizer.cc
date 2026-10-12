// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/input_visualizer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

constexpr base::TimeDelta kClickAnimationDuration = base::Milliseconds(350);

// Linear interpolation helper.
inline float Lerp(float start, float end, double progress) {
  return static_cast<float>(start + (end - start) * progress);
}

// Cubic ease-out curve.
inline double CubicEaseOut(double t) {
  double inv = 1.0 - t;
  return 1.0 - inv * inv * inv;
}

class InputVisualizerOverlayDelegate final : public FrameOverlay::Delegate {
 public:
  explicit InputVisualizerOverlayDelegate(InputVisualizer& visualizer)
      : visualizer_(&visualizer) {}

  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& context,
                         const gfx::Size& view_size) const override {
    if (visualizer_) {
      visualizer_->PaintFrameOverlay(frame_overlay, context, view_size);
    }
  }

  void Invalidate() override {
    if (visualizer_) {
      visualizer_->Invalidate();
    }
  }

 private:
  WeakPersistent<InputVisualizer> visualizer_;
};

}  // namespace

InputVisualizer::InputVisualizer(LocalFrame& frame) : frame_(&frame) {}

InputVisualizer::~InputVisualizer() = default;

void InputVisualizer::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(frame_overlay_);
}

void InputVisualizer::SetClicksEnabled(bool enabled) {
  if (clicks_enabled_ == enabled) {
    return;
  }
  clicks_enabled_ = enabled;
  if (!clicks_enabled_) {
    Shutdown();
  }
}

void InputVisualizer::HandleInputEvent(const WebInputEvent& event) {
  if (!clicks_enabled_) {
    return;
  }
  if (event.GetType() == WebInputEvent::Type::kPointerDown) {
    OnPointerDown(static_cast<const WebPointerEvent&>(event).PositionInWidget(),
                  event.TimeStamp());
  } else if (event.GetType() == WebInputEvent::Type::kMouseDown) {
    // CDP usually only emits isolated mouseDown events without explicit
    // gestures.
    const auto& mouse_event = static_cast<const WebMouseEvent&>(event);
    if (!mouse_event.FromTouch()) {
      OnPointerDown(mouse_event.PositionInWidget(), event.TimeStamp());
    }
  }
}

void InputVisualizer::OnPointerDown(const gfx::PointF& position_in_widget,
                                    base::TimeTicks timestamp) {
  if (!clicks_enabled_ || !frame_ || !frame_->View()) {
    return;
  }

  if (active_clicks_.size() >= kMaxConcurrentAnimations) {
    active_clicks_.EraseAt(0);
  }
  base::TimeTicks start_time =
      timestamp.is_null() ? base::TimeTicks::Now() : timestamp;
  active_clicks_.push_back(ClickPoint{position_in_widget, start_time});
  EnsureFrameOverlay();

  frame_->View()->SetVisualViewportOrOverlayNeedsRepaint();
  if (frame_->GetPage()) {
    frame_->GetPage()->GetChromeClient().ScheduleAnimation(frame_->View());
  }
}

void InputVisualizer::EnsureFrameOverlay() {
  if (!frame_overlay_ && frame_) {
    frame_overlay_ = MakeGarbageCollected<FrameOverlay>(
        frame_, std::make_unique<InputVisualizerOverlayDelegate>(*this));
  }
}

void InputVisualizer::DestroyFrameOverlay() {
  if (frame_overlay_) {
    frame_overlay_->Destroy();
    frame_overlay_ = nullptr;
  }
}

void InputVisualizer::UpdatePrePaint() {
  if (frame_overlay_) {
    frame_overlay_->UpdatePrePaint();
  }
}

void InputVisualizer::Paint(GraphicsContext& context) {
  if (frame_overlay_) {
    frame_overlay_->Paint(context);
  }
}

void InputVisualizer::Shutdown() {
  active_clicks_.clear();
  last_frame_time_ = base::TimeTicks();
  DestroyFrameOverlay();
}

void InputVisualizer::Invalidate() {
  if (frame_ && frame_->View()) {
    frame_->View()->SetVisualViewportOrOverlayNeedsRepaint();
  }
}

void InputVisualizer::ServiceScriptedAnimations(
    base::TimeTicks monotonic_frame_begin_time) {
  last_frame_time_ = monotonic_frame_begin_time;
  for (auto& click : active_clicks_) {
    if (click.start_time.is_null() ||
        monotonic_frame_begin_time < click.start_time) {
      click.start_time = monotonic_frame_begin_time;
    }
  }

  // Remove expired click animations.
  EraseIf(active_clicks_, [&](const ClickPoint& click) {
    return monotonic_frame_begin_time - click.start_time >
           kClickAnimationDuration;
  });

  // If there are still active animations, request the next animation frame.
  if (!active_clicks_.empty() && frame_ && frame_->View() &&
      frame_->GetPage()) {
    frame_->View()->SetVisualViewportOrOverlayNeedsRepaint();
    frame_->GetPage()->GetChromeClient().ScheduleAnimation(frame_->View());
  } else if (active_clicks_.empty()) {
    DestroyFrameOverlay();
  }
}

void InputVisualizer::PaintFrameOverlay(const FrameOverlay& frame_overlay,
                                        GraphicsContext& context,
                                        const gfx::Size& view_size) const {
  if (active_clicks_.empty()) {
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, frame_overlay,
                                                  DisplayItem::kFrameOverlay)) {
    return;
  }

  DrawingRecorder recorder(context, frame_overlay, DisplayItem::kFrameOverlay,
                           gfx::Rect(view_size));

  base::TimeTicks now =
      last_frame_time_.is_null() ? base::TimeTicks::Now() : last_frame_time_;

  for (const auto& click : active_clicks_) {
    base::TimeDelta elapsed = now - click.start_time;
    double progress = std::clamp(elapsed / kClickAnimationDuration, 0.0, 1.0);

    double eased_progress = CubicEaseOut(progress);

    float ring_radius = Lerp(4.0f, 26.0f, eased_progress);
    float stroke_width = Lerp(3.0f, 0.8f, progress);
    float alpha = Lerp(0.85f, 0.0f, progress);

    // 1. Semi-transparent background glow inside the ripple
    float fill_alpha = Lerp(0.20f, 0.0f, progress);
    cc::PaintFlags fill_flags;
    fill_flags.setAntiAlias(true);
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    fill_flags.setColor(SkColorSetARGB(
        base::ClampRound<uint8_t>(fill_alpha * 255), 0x1A, 0x73, 0xE8));
    gfx::RectF fill_rect(click.position.x() - ring_radius,
                         click.position.y() - ring_radius, ring_radius * 2,
                         ring_radius * 2);
    context.DrawOval(gfx::RectFToSkRect(fill_rect), fill_flags,
                     AutoDarkMode::Disabled());

    // 2. Main expanding ring
    cc::PaintFlags ring_flags;
    ring_flags.setAntiAlias(true);
    ring_flags.setStyle(cc::PaintFlags::kStroke_Style);
    ring_flags.setStrokeWidth(stroke_width);
    ring_flags.setColor(SkColorSetARGB(base::ClampRound<uint8_t>(alpha * 255),
                                       0x1A, 0x73, 0xE8));
    gfx::RectF ring_rect(click.position.x() - ring_radius,
                         click.position.y() - ring_radius, ring_radius * 2,
                         ring_radius * 2);
    context.DrawOval(gfx::RectFToSkRect(ring_rect), ring_flags,
                     AutoDarkMode::Disabled());

    // 3. Central burst dot during the initial half of animation
    if (progress < 0.5) {
      double core_progress = progress * 2.0;
      float core_radius = Lerp(3.0f, 7.0f, core_progress);
      float core_opacity = Lerp(0.7f, 0.0f, core_progress);

      cc::PaintFlags core_flags;
      core_flags.setAntiAlias(true);
      core_flags.setStyle(cc::PaintFlags::kFill_Style);
      core_flags.setColor(SkColorSetARGB(
          base::ClampRound<uint8_t>(core_opacity * 255), 0x1A, 0x73, 0xE8));
      gfx::RectF core_rect(click.position.x() - core_radius,
                           click.position.y() - core_radius, core_radius * 2,
                           core_radius * 2);
      context.DrawOval(gfx::RectFToSkRect(core_rect), core_flags,
                       AutoDarkMode::Disabled());
    }
  }
}

}  // namespace blink
