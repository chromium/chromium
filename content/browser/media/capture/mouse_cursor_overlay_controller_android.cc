// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "content/browser/media/capture/android_cursor_renderer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_android.h"
#include "ui/android/view_android_observer.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

// Observes mouse events and updates the cursor overlay.
// This is used to draw the mouse cursor for things such as tab sharing.
class MouseCursorOverlayController::Observer
    : public ui::EventForwarder::Observer,
      public ui::ViewAndroidObserver {
 public:
  Observer(MouseCursorOverlayController* controller, gfx::NativeView view)
      : controller_(controller), view_(view) {
    CHECK(controller_);
    CHECK(view_);

    controller_->OnMouseHasGoneIdle();
    view_->AddObserver(this);

    // Get contentview's event forwarder.
    gfx::NativeView parent = view_->parent();
    if (parent) {
      if (auto* event_forwarder = parent->event_forwarder()) {
        event_forwarder->AddObserver(this);
        observed_forwarder_ = event_forwarder;
      }
    }
  }

  ~Observer() override { StopTracking(); }

  void StopTracking() {
    if (observed_forwarder_) {
      observed_forwarder_->RemoveObserver(this);
      observed_forwarder_ = nullptr;
    }

    if (view_) {
      view_->RemoveObserver(this);
      view_ = nullptr;
      controller_->OnMouseHasGoneIdle();
    }
  }

  // ui::EventForwarder::Observer overrides:
  void OnMouseEvent(const ui::MotionEventAndroid& event) override {
    // We only care about mouse events.
    if (event.GetToolType(0) != ui::MotionEventAndroid::ToolType::MOUSE) {
      return;
    }

    // Request unbuffered dispatch to minimize latency and prevent Android from
    // batching hover/move events. This makes the cursor movement feel
    // significantly more responsive and is required to make cursor sharing
    // non-janky.
    view_->RequestUnbufferedDispatch(event);

    const gfx::PointF location_px(event.GetXPix(0), event.GetYPix(0));

    switch (event.GetAction()) {
      case ui::MotionEvent::Action::HOVER_MOVE:
      case ui::MotionEvent::Action::MOVE:
        controller_->OnMouseMoved(location_px);
        break;
      case ui::MotionEvent::Action::DOWN:
        controller_->OnMouseClicked(location_px);
        break;
      case ui::MotionEvent::Action::HOVER_ENTER:
        // Ensure cursor is shown when entering the view.
        controller_->OnMouseMoved(location_px);
        break;
      case ui::MotionEvent::Action::HOVER_EXIT:
        controller_->OnMouseHasGoneIdle();
        break;
      default:
        break;
    }
  }

  void OnTouchEvent(const ui::MotionEventAndroid& event) override {
    // Ignore touch events. The cursor overlay is strictly for mouse input.
  }

  // ui::ViewAndroidObserver overrides:
  void OnViewAndroidDestroyed() override { StopTracking(); }

  gfx::NativeView view() const { return view_; }

 private:
  raw_ptr<MouseCursorOverlayController> controller_;
  raw_ptr<ui::ViewAndroid> view_;
  // The specific forwarder we are observing (the parent's).
  raw_ptr<ui::EventForwarder> observed_forwarder_ = nullptr;
};

MouseCursorOverlayController::MouseCursorOverlayController()
    : mouse_activity_ended_timer_(
          FROM_HERE,
          kIdleTimeout,
          base::BindRepeating(&MouseCursorOverlayController::OnMouseHasGoneIdle,
                              base::Unretained(this))),
      mouse_move_behavior_atomic_(kNotMoving) {
  // MouseCursorOverlayController can be constructed on any thread, but
  // thereafter must be used according to class-level comments.
  DETACH_FROM_SEQUENCE(ui_sequence_checker_);
}

MouseCursorOverlayController::~MouseCursorOverlayController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  observer_.reset();
  Stop();
}

void MouseCursorOverlayController::SetTargetView(gfx::NativeView view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  observer_.reset();
  if (view) {
    observer_ = std::make_unique<Observer>(this, view);
  }
}

gfx::NativeCursor MouseCursorOverlayController::GetCurrentCursorOrDefault()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  float dip_scale = 1.0f;
  if (observer_ && observer_->view()) {
    dip_scale = observer_->view()->GetDipScale();
  }

  // We only support the custom arrow cursor that we generate.
  // Also we cache this and reuse it, and just regenerate if the scale changes.
  struct CursorCache {
    float scale = 0.0f;
    ui::Cursor cursor;
  };
  static base::NoDestructor<CursorCache> cache;

  // Use a small epsilon for floating point comparison (since DIPs are floats)
  constexpr float kScaleEpsilon = 0.01f;
  if (!base::IsApproximatelyEqual(cache->scale, dip_scale, kScaleEpsilon)) {
    SkBitmap bitmap = AndroidCursorRenderer::GenerateCursorImage(dip_scale);

    // The hotspot returned by the AndroidCursorRenderer is in logical DIPs.
    // It needs to be scaled to physical pixels to match the bitmap resolution.
    const gfx::Point logical_hotspot =
        AndroidCursorRenderer::GetCursorHotspot();
    const gfx::Point hotspot_px =
        gfx::ScaleToRoundedPoint(logical_hotspot, dip_scale);

    cache->cursor =
        ui::Cursor::NewCustom(std::move(bitmap), hotspot_px, dip_scale);
    cache->scale = dip_scale;
  }

  return cache->cursor;
}

gfx::RectF MouseCursorOverlayController::ComputeRelativeBoundsForOverlay(
    const gfx::NativeCursor& cursor,
    const gfx::PointF& location_px) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  gfx::Size shared_surface_size_px;
  float dip_scale = 1.0f;

  if (observer_ && observer_->view()) {
    shared_surface_size_px = observer_->view()->GetPhysicalBackingSize();
    dip_scale = observer_->view()->GetDipScale();
  } else if (!target_size_.IsEmpty()) {
    // target_size_ is used + set in tests. This is used in tests because during
    // testing, it explicitly disconnects the controller from the view. My
    // understanding here is that this prevents actual mouse events from being
    // dispatched, and thus only the simulated events are used.
    shared_surface_size_px = target_size_;
  } else {
    return gfx::RectF();
  }

  if (shared_surface_size_px.IsEmpty()) {
    return gfx::RectF();
  }

  const SkBitmap& bitmap = cursor.custom_bitmap();
  CHECK(!bitmap.empty());

  // 1. Convert the cursor's physical bitmap dimensions and hotspot back into
  // logical DIPs by dividing them by the cursor's inherent image scale factor.
  // (That's the same as the DIP scale factor used to generate the bitmap)
  const float scale = cursor.image_scale_factor();
  const gfx::SizeF cursor_size_dips =
      gfx::ScaleSize(gfx::SizeF(bitmap.width(), bitmap.height()), 1.0f / scale);
  const gfx::PointF hotspot_dips =
      gfx::ScalePoint(gfx::PointF(cursor.custom_hotspot()), 1.0f / scale);

  // 2. Scale the logical DIP dimensions to the target view's physical pixel
  // space. If the screen is high-density (e.g. dip_scale = 2.0f), a
  // 24x24 DIP cursor becomes 48x48 physical pixels on the video frame.
  const gfx::SizeF cursor_size_px = gfx::ScaleSize(cursor_size_dips, dip_scale);

  // 3. Similarly, scale the logical hotspot to the target view's physical
  // pixel space to find its exact pixel offset within the scaled cursor image.
  const gfx::PointF hotspot_px = gfx::ScalePoint(hotspot_dips, dip_scale);

  // 4. Center the image. location_px is the exact (x, y) coordinate of
  // the mouse pointer in physical screen pixels. We subtract hotspot_px
  // from location_px to find the physical coordinate where the top-left
  // corner of the image should be drawn.
  const gfx::RectF bounds_px(location_px - hotspot_px.OffsetFromOrigin(),
                             cursor_size_px);

  // 5. Compress into relative bounds. The video capture requires relative
  // coordinates from 0.0 (0%) to 1.0 (100%). We divide by the shared surface
  // size to convert physical pixels into a scalar ratio.
  return gfx::ScaleRect(bounds_px, 1.0f / shared_surface_size_px.width(),
                        1.0f / shared_surface_size_px.height());
}

void MouseCursorOverlayController::DisconnectFromToolkitForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  if (observer_) {
    observer_->StopTracking();
  }
}

// static
SkBitmap MouseCursorOverlayController::GetCursorImage(
    const gfx::NativeCursor& cursor) {
  return cursor.custom_bitmap();
}

}  // namespace content
