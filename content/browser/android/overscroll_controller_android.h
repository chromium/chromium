// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host.h"
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
class MotionEventAndroid;
class WindowAndroidCompositor;
struct DidOverscrollParams;
}

namespace content {

// Glue class for handling all inputs into Android-specific overscroll effects,
// both the passive overscroll glow and the active overscroll pull-to-refresh.
// Note that all input coordinates (both for events and overscroll) are in DIPs.
class CONTENT_EXPORT OverscrollControllerAndroid
    : public ui::OverscrollGlowClient,
      public RenderWidgetHost::InputEventObserver {
 public:
  OverscrollControllerAndroid(
      ui::OverscrollRefreshHandler* overscroll_refresh_handler,
      ui::WindowAndroidCompositor* compositor,
      float dpi_scale,
      RenderWidgetHost* host);

  static std::unique_ptr<OverscrollControllerAndroid> CreateForTests(
      ui::WindowAndroidCompositor* compositor,
      float dpi_scale,
      std::unique_ptr<ui::OverscrollGlow> glow_effect,
      std::unique_ptr<ui::OverscrollRefresh> refresh_effect);

  OverscrollControllerAndroid(const OverscrollControllerAndroid&) = delete;
  OverscrollControllerAndroid& operator=(const OverscrollControllerAndroid&) =
      delete;

  ~OverscrollControllerAndroid() override;

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

  void SetTouchpadOverscrollHistoryNavigation(bool enabled);

  // Returns true if the controller is actively handling the current input
  // sequence. This state persists until reset by
  // MotionEventAndroid::Action::DOWN from the next input sequence.
  bool IsHandlingInputSequence();

  // Returns true if |event| is consumed by an overscroll effect, in which
  // case it should cease propagation.
  bool OnTouchEvent(const ui::MotionEventAndroid& event);

  // Start RenderWidgetHost::InputEventObserver overrides
  void OnInputEvent(const RenderWidgetHost& widget,
                    const blink::WebInputEvent&) override;
  void OnInputEventAck(const RenderWidgetHost& widget,
                       blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent&) override;
  // End RenderWidgetHost::InputEventObserver overrides

 private:
  FRIEND_TEST_ALL_PREFIXES(OverscrollControllerAndroidUnitTest,
                           ConsumedBeginDoesNotResetEnabledRefresh);

  bool ShouldHandleInputEvents();
  void OnGestureEvent(const blink::WebGestureEvent& event);

  // To be called upon receipt of a gesture event ack.
  void OnGestureEventAck(const blink::WebGestureEvent& event,
                         blink::mojom::InputEventResultState ack_result);

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

  // True if the OverscrollController has claimed the current input sequence. It
  // will continue handling all events in this sequence until a terminating
  // action (ACTION_UP/ACTION_CANCEL) occurs.
  bool is_handling_sequence_ = false;

  // Stores the last seen position of a touch input event (in pix) to correctly
  // calculate scroll deltas for `refresh_effect_`.
  gfx::Vector2dF last_pos_;

  // TODO(jdduke): Factor out a common API from the two overscroll effects.
  std::unique_ptr<ui::OverscrollGlow> glow_effect_;
  std::unique_ptr<ui::OverscrollRefresh> refresh_effect_;
  base::ScopedObservation<RenderWidgetHost,
                          RenderWidgetHost::InputEventObserver>
      obs_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_
