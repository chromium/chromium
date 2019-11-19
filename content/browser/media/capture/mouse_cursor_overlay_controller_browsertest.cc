// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace content {
namespace {

class FakeOverlay : public MouseCursorOverlayController::Overlay {
 public:
  FakeOverlay() = default;
  ~FakeOverlay() final = default;

  const SkBitmap& image() const { return image_; }
  const gfx::RectF& bounds() const { return bounds_; }

  void SetImageAndBounds(const SkBitmap& image,
                         const gfx::RectF& bounds) final {
    image_ = image;
    bounds_ = bounds;
  }

  void SetBounds(const gfx::RectF& bounds) final { bounds_ = bounds; }

 private:
  SkBitmap image_;
  gfx::RectF bounds_;

  DISALLOW_COPY_AND_ASSIGN(FakeOverlay);
};

}  // namespace

class MouseCursorOverlayControllerBrowserTest : public ContentBrowserTest {
 public:
  MouseCursorOverlayControllerBrowserTest() = default;
  ~MouseCursorOverlayControllerBrowserTest() override = default;

  void SetUpOnMainThread() final {
    ContentBrowserTest::SetUpOnMainThread();
    controller_.SetTargetView(shell()->web_contents()->GetNativeView());
    controller_.DisconnectFromToolkitForTesting();
    base::RunLoop().RunUntilIdle();
  }

  FakeOverlay* Start() {
    auto overlay_ptr = std::make_unique<FakeOverlay>();
    FakeOverlay* const overlay = overlay_ptr.get();
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    controller_.Start(std::move(overlay_ptr),
                      base::CreateSingleThreadTaskRunner({BrowserThread::UI}));
    return overlay;
  }

  gfx::PointF GetLowerRightMostPointInsideView() {
    const gfx::Size& view_size = GetAbsoluteViewSize();
    return gfx::PointF(1.0f - 1.0f / view_size.width(),
                       1.0f - 1.0f / view_size.height());
  }

  void SimulateMouseTravel(float from_x, float from_y, float to_x, float to_y) {
    constexpr int kNumMoves = 10;
    for (int i = kNumMoves; i >= 0; --i) {
      const float t = static_cast<float>(i) / kNumMoves;
      const float x = t * from_x + (1.0f - t) * to_x;
      const float y = t * from_y + (1.0f - t) * to_y;
      controller_.OnMouseMoved(ToAbsoluteLocationInView(x, y));
    }
    base::RunLoop().RunUntilIdle();
  }

  void SimulateMouseClick(float x, float y) {
    controller_.OnMouseClicked(ToAbsoluteLocationInView(x, y));
    base::RunLoop().RunUntilIdle();
  }

  void SimulateMouseHasGoneIdle() {
    controller_.OnMouseHasGoneIdle();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(controller_.mouse_activity_ended_timer_.IsRunning());
  }

  void SimulateUnintentionalMouseMovement(float x, float y) {
    const gfx::Size& view_size = GetAbsoluteViewSize();
    const float distance_x =
        (0.5f * MouseCursorOverlayController::kMinMovementPixels) /
        view_size.width();
    const float distance_y =
        (0.5f * MouseCursorOverlayController::kMinMovementPixels) /
        view_size.height();
    DoSquareDance(x, y, distance_x, distance_y);
  }

  void SimulateBarelyIntentionalMouseMovement(float x, float y) {
    const gfx::Size& view_size = GetAbsoluteViewSize();
    const float distance_x =
        (1.5f * MouseCursorOverlayController::kMinMovementPixels) /
        view_size.width();
    const float distance_y =
        (1.5f * MouseCursorOverlayController::kMinMovementPixels) /
        view_size.height();
    DoSquareDance(x, y, distance_x, distance_y);
  }

  void ExpectOverlayPositionedAt(const FakeOverlay& overlay,
                                 float expected_x,
                                 float expected_y) {
    const gfx::SizeF& overlay_size = GetExpectedOverlaySize();
    // The position will be slightly off because of the hotspot offset.
    EXPECT_NEAR(expected_x, overlay.bounds().x(), overlay_size.width() / 2.0f);
    EXPECT_NEAR(expected_y, overlay.bounds().y(), overlay_size.height() / 2.0f);
  }

  void ExpectOverlaySizeMatchesCurrentCursor(const FakeOverlay& overlay) const {
    const gfx::SizeF& expected_size = GetExpectedOverlaySize();
    EXPECT_FALSE(expected_size.IsEmpty());
    EXPECT_FALSE(overlay.image().drawsNothing());
    EXPECT_NEAR(expected_size.width(), overlay.bounds().width(), 0.001);
    EXPECT_NEAR(expected_size.height(), overlay.bounds().height(), 0.001);
  }

  bool IsUserInteractingWithView() const {
    return controller_.IsUserInteractingWithView();
  }

 private:
  gfx::Size GetAbsoluteViewSize() const {
    const gfx::Size view_size =
        shell()->web_contents()->GetContainerBounds().size();
    CHECK(!view_size.IsEmpty());
    return view_size;
  }

  gfx::PointF ToAbsoluteLocationInView(float relative_x, float relative_y) {
    const gfx::Size& view_size = GetAbsoluteViewSize();
    return gfx::PointF(relative_x * view_size.width(),
                       relative_y * view_size.height());
  }

  gfx::SizeF GetExpectedOverlaySize() const {
    const gfx::Size& view_size = GetAbsoluteViewSize();
    const SkBitmap image =
        controller_.GetCursorImage(controller_.GetCurrentCursorOrDefault());
    return gfx::SizeF(static_cast<float>(image.width()) / view_size.width(),
                      static_cast<float>(image.height()) / view_size.height());
  }

  void DoSquareDance(float x, float y, float distance_x, float distance_y) {
    SimulateMouseTravel(x, y, x + distance_x, y);
    SimulateMouseTravel(x + distance_x, y, x + distance_x, y + distance_y);
    SimulateMouseTravel(x + distance_x, y + distance_y, x, y + distance_y);
    SimulateMouseTravel(x, y + distance_y, x, y);
    base::RunLoop().RunUntilIdle();
  }

  MouseCursorOverlayController controller_;

  DISALLOW_COPY_AND_ASSIGN(MouseCursorOverlayControllerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MouseCursorOverlayControllerBrowserTest,
                       PositionsOverlayOnMouseMoves) {
  FakeOverlay* const overlay = Start();

  // Cursor not showing at start.
  EXPECT_TRUE(overlay->image().drawsNothing());
  EXPECT_TRUE(overlay->bounds().IsEmpty());
  EXPECT_FALSE(IsUserInteractingWithView());

  // Move to upper-leftmost corner.
  {
    SCOPED_TRACE(testing::Message() << "upper-leftmost corner of view");
    SimulateMouseTravel(0.5f, 0.5f, 0.0f, 0.0f);
    ExpectOverlayPositionedAt(*overlay, 0.0f, 0.0f);
    ExpectOverlaySizeMatchesCurrentCursor(*overlay);
    EXPECT_TRUE(IsUserInteractingWithView());
  }

  // Move to middle.
  {
    SCOPED_TRACE(testing::Message() << "center of view");
    SimulateMouseTravel(0.0f, 0.0f, 0.5f, 0.5f);
    ExpectOverlayPositionedAt(*overlay, 0.5f, 0.5f);
    ExpectOverlaySizeMatchesCurrentCursor(*overlay);
    EXPECT_TRUE(IsUserInteractingWithView());
  }

  // Move to lower-rightmost corner.
  {
    SCOPED_TRACE(testing::Message() << "lower-rightmost corner of view");
    const gfx::PointF lower_right = GetLowerRightMostPointInsideView();
    SimulateMouseTravel(0.5f, 0.5f, lower_right.x(), lower_right.y());
    ExpectOverlayPositionedAt(*overlay, lower_right.x(), lower_right.y());
    ExpectOverlaySizeMatchesCurrentCursor(*overlay);
    EXPECT_TRUE(IsUserInteractingWithView());
  }
}

IN_PROC_BROWSER_TEST_F(MouseCursorOverlayControllerBrowserTest,
                       PositionsOverlayOnMouseClicks) {
  FakeOverlay* const overlay = Start();

  // Cursor not showing at start.
  EXPECT_TRUE(overlay->bounds().IsEmpty());
  EXPECT_FALSE(IsUserInteractingWithView());

  // Click in the middle of the view.
  SimulateMouseClick(0.5f, 0.5f);
  ExpectOverlayPositionedAt(*overlay, 0.5f, 0.5f);
  ExpectOverlaySizeMatchesCurrentCursor(*overlay);
  EXPECT_TRUE(IsUserInteractingWithView());
}

IN_PROC_BROWSER_TEST_F(MouseCursorOverlayControllerBrowserTest,
                       CursorHidesWhenMouseStopsMoving) {
  FakeOverlay* const overlay = Start();

  // Cursor not showing at start.
  EXPECT_TRUE(overlay->bounds().IsEmpty());
  EXPECT_FALSE(IsUserInteractingWithView());

  // Move to middle.
  SimulateMouseTravel(0.0f, 0.0f, 0.5f, 0.5f);
  ExpectOverlayPositionedAt(*overlay, 0.5f, 0.5f);
  ExpectOverlaySizeMatchesCurrentCursor(*overlay);
  EXPECT_TRUE(IsUserInteractingWithView());

  // Simulate no movement for the timeout period.
  SimulateMouseHasGoneIdle();
  EXPECT_TRUE(overlay->bounds().IsEmpty());
  EXPECT_FALSE(IsUserInteractingWithView());

  // Move the mouse a little, but not enough to trip the "intentionally moved"
  // logic.
  SimulateUnintentionalMouseMovement(0.5f, 0.5f);
  EXPECT_TRUE(overlay->bounds().IsEmpty());
  EXPECT_FALSE(IsUserInteractingWithView());

  // Move the mouse just a bit more, to trip the "intentionally moved" logic.
  SimulateBarelyIntentionalMouseMovement(0.5f, 0.5f);
  ExpectOverlayPositionedAt(*overlay, 0.5f, 0.5f);
  ExpectOverlaySizeMatchesCurrentCursor(*overlay);
  EXPECT_TRUE(IsUserInteractingWithView());
}

}  // namespace content
