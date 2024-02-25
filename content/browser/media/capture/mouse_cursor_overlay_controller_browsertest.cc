// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/wm/core/cursor_loader.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::Mock;

namespace content {

namespace {

const base::TimeDelta kMinWaitInterval = base::Milliseconds(1000 / (30 - 1));

class MockOverlay final : public MouseCursorOverlayController::Overlay {
 public:
  MockOverlay() = default;

  MockOverlay(const MockOverlay&) = delete;
  MockOverlay& operator=(const MockOverlay&) = delete;

  ~MockOverlay() override = default;

  const SkBitmap& image() const { return image_; }
  const gfx::RectF& bounds() const { return bounds_; }

  void SetImageAndBounds(const SkBitmap& image,
                         const gfx::RectF& bounds) final {
    image_ = image;
    bounds_ = bounds;
  }

  void SetBounds(const gfx::RectF& bounds) final { bounds_ = bounds; }
  MOCK_METHOD1(OnCapturedMouseEvent, void((const gfx::Point&)));

 private:
  SkBitmap image_;
  gfx::RectF bounds_;
};

}  // namespace

class MouseCursorOverlayControllerBrowserTest : public ContentBrowserTest {
 public:
  MouseCursorOverlayControllerBrowserTest() = default;

  MouseCursorOverlayControllerBrowserTest(
      const MouseCursorOverlayControllerBrowserTest&) = delete;
  MouseCursorOverlayControllerBrowserTest& operator=(
      const MouseCursorOverlayControllerBrowserTest&) = delete;

  ~MouseCursorOverlayControllerBrowserTest() override = default;

  virtual void InitFeatures() {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {blink::features::kCapturedMouseEvents},
        /* disabled_features */ {});
  }

  void SetUp() final {
    InitFeatures();
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() final {
    ContentBrowserTest::SetUpOnMainThread();

    // On Ash content browsertests, ash::Shell isn't initialized and thus
    // neither NativeCursorManagerAsh, the owner of CursorLoader.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    cursor_loader_ = std::make_unique<wm::CursorLoader>();
    aura::client::SetCursorShapeClient(cursor_loader_.get());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    controller_.SetTargetView(shell()->web_contents()->GetNativeView());
    controller_.DisconnectFromToolkitForTesting();
    base::RunLoop().RunUntilIdle();
  }

  MockOverlay* Start(const base::TickClock* tick_clock = nullptr) {
    auto overlay_ptr = std::make_unique<MockOverlay>();
    MockOverlay* const overlay = overlay_ptr.get();
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    controller_.Start(std::move(overlay_ptr), GetUIThreadTaskRunner({}));
    if (tick_clock) {
      controller_.SetTickClockForTesting(tick_clock);
    }
    return overlay;
  }

  gfx::PointF GetLowerRightMostPointInsideView() {
    const gfx::Size& view_size = GetAbsoluteViewSize();
    return gfx::PointF(1.0f - 1.0f / view_size.width(),
                       1.0f - 1.0f / view_size.height());
  }

  void Wait(base::TimeDelta timeout) {
    base::RunLoop run_loop;
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, timeout, run_loop.QuitClosure());
    run_loop.Run();
    timer.Stop();
  }

  void SimulateMouseCoordinatesUpdated(gfx::Point coordinates) {
    if (controller_.ShouldSendMouseEvents()) {
      controller_.OnMouseCoordinatesUpdated(coordinates);
    }
    base::RunLoop().RunUntilIdle();
  }

  void SimulateMouseCoordinatesUpdatedAndWaitForCallback(
      MockOverlay& overlay,
      gfx::Point coordinates) {
    base::RunLoop run_loop;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(overlay, OnCapturedMouseEvent(coordinates))
        .WillOnce([&quit_closure](const gfx::Point&) {
          std::move(quit_closure).Run();
        });
    SimulateMouseCoordinatesUpdated(coordinates);
    run_loop.Run();
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

  void ExpectOverlayPositionedAt(const MockOverlay& overlay,
                                 float expected_x,
                                 float expected_y) {
    const gfx::SizeF& overlay_size = GetExpectedOverlaySize();
    // The position will be slightly off because of the hotspot offset.
    EXPECT_NEAR(expected_x, overlay.bounds().x(), overlay_size.width() / 2.0f);
    EXPECT_NEAR(expected_y, overlay.bounds().y(), overlay_size.height() / 2.0f);
  }

  void ExpectOverlaySizeMatchesCurrentCursor(const MockOverlay& overlay) const {
    const gfx::SizeF& expected_size = GetExpectedOverlaySize();
    EXPECT_FALSE(expected_size.IsEmpty());
    EXPECT_FALSE(overlay.image().drawsNothing());
    EXPECT_NEAR(expected_size.width(), overlay.bounds().width(), 0.001);
    EXPECT_NEAR(expected_size.height(), overlay.bounds().height(), 0.001);
  }

  bool IsUserInteractingWithView() const {
    return controller_.IsUserInteractingWithView();
  }

  gfx::Size GetAbsoluteViewSize() const {
    const gfx::Size view_size =
        shell()->web_contents()->GetContainerBounds().size();
    CHECK(!view_size.IsEmpty());
    return view_size;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<wm::CursorLoader> cursor_loader_;
#endif
};

IN_PROC_BROWSER_TEST_F(MouseCursorOverlayControllerBrowserTest,
                       PositionsOverlayOnMouseMoves) {
  MockOverlay* const overlay = Start();

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
  MockOverlay* const overlay = Start();

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
  MockOverlay* const overlay = Start();

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

// This test verifies that MouseCoordinatesUpdated calls are forwarded to the
// overlay.
IN_PROC_BROWSER_TEST_F(MouseCursorOverlayControllerBrowserTest,
                       ForwardMouseEvents) {
  MockOverlay* const overlay = Start();
  Mock::AllowLeak(overlay);
  const gfx::Rect rect(GetAbsoluteViewSize());

  // Simulate a mouse coordinates update at the upper-left corner.
  SimulateMouseCoordinatesUpdatedAndWaitForCallback(*overlay, rect.origin());

  // Simulate a mouse coordinates update outside the view.
  SimulateMouseCoordinatesUpdatedAndWaitForCallback(*overlay,
                                                    gfx::Point(-1, -1));

  // Simulate a mouse coordinates update at center.
  SimulateMouseCoordinatesUpdatedAndWaitForCallback(*overlay,
                                                    rect.CenterPoint());

  // Simulate a mouse coordinates update at the lower-right corner.
  SimulateMouseCoordinatesUpdatedAndWaitForCallback(*overlay,
                                                    rect.bottom_right());

  Mock::VerifyAndClearExpectations(overlay);
}

// This test verifies calls to MouseCoordinatesUpdated that don't change the
// coordinates are not forwarded to the overlay.
IN_PROC_BROWSER_TEST_F(MouseCursorOverlayControllerBrowserTest,
                       MouseEventsNotFiredIfNotMoved) {
  MockOverlay* const overlay = Start();
  Mock::AllowLeak(overlay);

  const gfx::Point center = gfx::Rect(GetAbsoluteViewSize()).CenterPoint();

  // Simulate a mouse coordinates update at center.
  SimulateMouseCoordinatesUpdatedAndWaitForCallback(*overlay, center);

  // Simulate another mouse coordinates update at center, it should be ignored.
  {
    EXPECT_CALL(*overlay, OnCapturedMouseEvent(center)).Times(0);
    SimulateMouseCoordinatesUpdated(center);
    Wait(base::Seconds(1));  // Wait a bit to make sure the event would arrive.
  }

  // Try again after the internal minimal wait interval.
  {
    Wait(kMinWaitInterval);
    EXPECT_CALL(*overlay, OnCapturedMouseEvent(center)).Times(0);
    SimulateMouseCoordinatesUpdated(center);
    Wait(base::Seconds(1));  // Wait a bit to make sure the event would arrive.
  }

  Mock::VerifyAndClearExpectations(overlay);
}

// This test verifies the minimal wait interval used between two calls
// to MouseCoordinatesUpdated.
IN_PROC_BROWSER_TEST_F(MouseCursorOverlayControllerBrowserTest,
                       MouseEventsEnsureMinWaitInterval) {
  base::SimpleTestTickClock test_clock;
  MockOverlay* const overlay = Start(&test_clock);
  Mock::AllowLeak(overlay);

  const gfx::Rect rect(GetAbsoluteViewSize());
  const gfx::Point p1 = rect.CenterPoint();
  const gfx::Point p2 = rect.origin();
  const gfx::Point p3 = rect.top_right();
  const gfx::Point p4 = rect.bottom_right();
  const gfx::Point p5 = rect.bottom_left();
  const gfx::Point p6(1, -1);

  EXPECT_CALL(*overlay, OnCapturedMouseEvent(p1)).Times(1);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(p2)).Times(0);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(p3)).Times(1);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(p4)).Times(1);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(p5)).Times(0);
  EXPECT_CALL(*overlay, OnCapturedMouseEvent(p6)).Times(1);

  // No previous event sent to OnCapturedMouseEvent, dispatch p1 immediately.
  bool callback1_called = false;
  ON_CALL(*overlay, OnCapturedMouseEvent(p1))
      .WillByDefault(testing::Assign(&callback1_called, true));
  SimulateMouseCoordinatesUpdated(p1);
  EXPECT_TRUE(callback1_called);

  // p1 dispatched less than kMinWaitInterval ago, so p2 is buffered.
  test_clock.Advance(kMinWaitInterval / 2);
  SimulateMouseCoordinatesUpdated(p2);
  base::RunLoop().RunUntilIdle();

  // p1 dispatched less than kMinWaitInterval ago, so p3 is buffered too,
  // overriding p2.
  bool callback3_called = false;
  base::RunLoop run_loop_3;
  base::OnceClosure quit_closure_3 = run_loop_3.QuitClosure();
  ON_CALL(*overlay, OnCapturedMouseEvent(p3))
      .WillByDefault([&callback3_called, &quit_closure_3](const gfx::Point&) {
        callback3_called = true;
        std::move(quit_closure_3).Run();
      });
  SimulateMouseCoordinatesUpdated(p3);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback3_called);

  // No events dispatched less than kMinWaitInterval ago, so p3 is dispatched.
  test_clock.Advance(kMinWaitInterval / 2);
  run_loop_3.Run();
  EXPECT_TRUE(callback3_called);

  // No events dispatched less than kMinWaitInterval ago or buffered and none
  // buffered, so dispatch p4 immediately.
  test_clock.Advance(kMinWaitInterval);
  bool callback4_called = false;
  ON_CALL(*overlay, OnCapturedMouseEvent(p4))
      .WillByDefault(testing::Assign(&callback4_called, true));
  SimulateMouseCoordinatesUpdated(p4);
  EXPECT_TRUE(callback4_called);

  // p4 dispatched less than kMinWaitInterval ago, so p5 is buffered.
  test_clock.Advance(kMinWaitInterval / 2);
  SimulateMouseCoordinatesUpdated(p5);
  base::RunLoop().RunUntilIdle();

  // p4 dispatched less than kMinWaitInterval ago, so p6 is buffered too,
  // overriding p5.
  bool callback6_called = false;
  base::RunLoop run_loop_6;
  base::OnceClosure quit_closure_6 = run_loop_6.QuitClosure();
  ON_CALL(*overlay, OnCapturedMouseEvent(p6))
      .WillByDefault([&callback6_called, &quit_closure_6](const gfx::Point&) {
        callback6_called = true;
        std::move(quit_closure_6).Run();
      });
  SimulateMouseCoordinatesUpdated(p6);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback6_called);

  // No events dispatched less than kMinWaitInterval ago, so p6 is dispatched.
  test_clock.Advance(kMinWaitInterval / 2);
  run_loop_6.Run();
  EXPECT_TRUE(callback6_called);

  // No more events should be dispatched later.
  test_clock.Advance(base::Seconds(5));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(overlay);
}

class MouseCursorOverlayControllerNoMouseEventsBrowserTest
    : public MouseCursorOverlayControllerBrowserTest {
 public:
  MouseCursorOverlayControllerNoMouseEventsBrowserTest() = default;

  MouseCursorOverlayControllerNoMouseEventsBrowserTest(
      const MouseCursorOverlayControllerNoMouseEventsBrowserTest&) = delete;
  MouseCursorOverlayControllerNoMouseEventsBrowserTest& operator=(
      const MouseCursorOverlayControllerNoMouseEventsBrowserTest&) = delete;

  ~MouseCursorOverlayControllerNoMouseEventsBrowserTest() override = default;

  void InitFeatures() final {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {},
        /* disabled_features */ {blink::features::kCapturedMouseEvents});
  }
};

// This test verifies calls to MouseCoordinatesUpdated are not forwarded if the
// CapturedMouseEvents preference is disabled.
IN_PROC_BROWSER_TEST_F(MouseCursorOverlayControllerNoMouseEventsBrowserTest,
                       DoNotForwardMouseEvents) {
  MockOverlay* const overlay = Start();
  Mock::AllowLeak(overlay);

  const gfx::Point center = gfx::Rect(GetAbsoluteViewSize()).CenterPoint();

  // Simulate a mouse coordinates update at center, it should be ignored.
  {
    EXPECT_CALL(*overlay, OnCapturedMouseEvent(center)).Times(0);
    SimulateMouseCoordinatesUpdated(center);
    Wait(base::Seconds(1));  // Wait a bit to make sure the event would arrive.
  }

  Mock::VerifyAndClearExpectations(overlay);
}

}  // namespace content
