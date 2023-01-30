// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/android/overscroll_glow.h"
#include "ui/android/overscroll_refresh.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
class WebGestureEvent;
}

namespace cc::slim {
class Layer;
}

namespace ui {
class WindowAndroidCompositor;
struct DidOverscrollParams;
}

namespace content {

// Glue class for handling all inputs into Android-specific overscroll effects,
// both the passive overscroll glow and the active overscroll pull-to-refresh.
// Note that all input coordinates (both for events and overscroll) are in DIPs.
class CONTENT_EXPORT OverscrollControllerAndroid
    : public ui::OverscrollGlowClient {
 public:
  OverscrollControllerAndroid(
      ui::OverscrollRefreshHandler* overscroll_refresh_handler,
      ui::WindowAndroidCompositor* compositor,
      float dpi_scale);

  static std::unique_ptr<OverscrollControllerAndroid> CreateForTests(
      ui::WindowAndroidCompositor* compositor,
      float dpi_scale,
      std::unique_ptr<ui::OverscrollGlow> glow_effect,
      std::unique_ptr<ui::OverscrollRefresh> refresh_effect);

  OverscrollControllerAndroid(const OverscrollControllerAndroid&) = delete;
  OverscrollControllerAndroid& operator=(const OverscrollControllerAndroid&) =
      delete;

  ~OverscrollControllerAndroid() override;

  // Returns true if |event| is consumed by an overscroll effect, in which
  // case it should cease propagation.
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event);

  // To be called upon receipt of a gesture event ack.
  void OnGestureEventAck(const blink::WebGestureEvent& event,
                         blink::mojom::InputEventResultState ack_result);

  // To be called upon receipt of an overscroll event.
  void OnOverscrolled(const ui::DidOverscrollParams& overscroll_params);

  // Returns true if the effect still needs animation ticks.
  // Note: The effect will detach itself when no further animation is required.
  bool Animate(base::TimeTicks current_time, cc::slim::Layer* parent_layer);

  // To be called whenever the content frame has been updated.
  void OnFrameMetadataUpdated(float page_scale_factor,
                              float device_scale_factor,
                              const gfx::SizeF& scrollable_viewport_size,
                              const gfx::SizeF& root_layer_size,
                              const gfx::PointF& root_scroll_offset,
                              bool root_overflow_y_hidden);

  // Toggle activity of any overscroll effects. When disabled, events will be
  // ignored until the controller is re-enabled.
  void Enable();
  void Disable();

 private:
  // This method should only be called from CreateForTests.
  OverscrollControllerAndroid(
      ui::WindowAndroidCompositor* compositor,
      float dpi_scale,
      std::unique_ptr<ui::OverscrollGlow> glow_effect,
      std::unique_ptr<ui::OverscrollRefresh> refresh_effect);

  // OverscrollGlowClient implementation.
  std::unique_ptr<ui::EdgeEffect> CreateEdgeEffect() override;

  void SetNeedsAnimate();

  const raw_ptr<ui::WindowAndroidCompositor, DanglingUntriaged> compositor_;
  const float dpi_scale_;

  bool enabled_;

  // TODO(jdduke): Factor out a common API from the two overscroll effects.
  std::unique_ptr<ui::OverscrollGlow> glow_effect_;
  std::unique_ptr<ui::OverscrollRefresh> refresh_effect_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_
