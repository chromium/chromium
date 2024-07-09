// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_bezier.h"

#include "build/build_config.h"
#include "cc/input/input_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

using gfx::Size;
using gfx::Vector2dF;
using PhaseState = WebGestureEvent::InertialPhaseState;

class MockScrollElasticityHelper : public cc::ScrollElasticityHelper {
 public:
  MockScrollElasticityHelper() = default;
  ~MockScrollElasticityHelper() override = default;

  // cc::ScrollElasticityHelper implementation:
  Size ScrollBounds() const override { return Size(1000, 1000); }
  bool IsUserScrollableHorizontal() const override { return true; }
  bool IsUserScrollableVertical() const override { return true; }
  Vector2dF StretchAmount() const override { return stretch_amount_; }
  void SetStretchAmount(const Vector2dF& stretch_amount) override {
    stretch_amount_ = stretch_amount;
  }
  void ScrollBy(const Vector2dF& delta) override { scroll_offset_ += delta; }
  void RequestOneBeginFrame() override {}
  gfx::PointF ScrollOffset() const override { return scroll_offset_; }
  gfx::PointF MaxScrollOffset() const override { return max_scroll_offset_; }

  void SetScrollOffsetAndMaxScrollOffset(const gfx::PointF& scroll_offset,
                                         const gfx::PointF& max_scroll_offset) {
    scroll_offset_ = scroll_offset;
    max_scroll_offset_ = max_scroll_offset;
  }

 private:
  Vector2dF stretch_amount_;
  gfx::PointF scroll_offset_, max_scroll_offset_;
};

class ElasticOverscrollControllerBezierTest : public testing::Test {
 public:
  ElasticOverscrollControllerBezierTest() : controller_(&helper_) {}
  ~ElasticOverscrollControllerBezierTest() override = default;

  void SetUp() override {}

  void SendGestureScrollBegin(PhaseState inertialPhase) {
    WebGestureEvent event(WebInputEvent::Type::kGestureScrollBegin,
                          WebInputEvent::kNoModifiers, base::TimeTicks(),
                          WebGestureDevice::kTouchpad);
    event.data.scroll_begin.inertial_phase = inertialPhase;

    controller_.ObserveGestureEventAndResult(event,
                                             cc::InputHandlerScrollResult());
  }

  void SendGestureScrollUpdate(PhaseState inertialPhase,
                               const Vector2dF& unused_scroll_delta) {
    blink::WebGestureEvent event(WebInputEvent::Type::kGestureScrollUpdate,
                                 WebInputEvent::kNoModifiers, base::TimeTicks(),
                                 blink::WebGestureDevice::kTouchpad);
    event.data.scroll_update.inertial_phase = inertialPhase;
    cc::InputHandlerScrollResult scroll_result;
    scroll_result.did_overscroll_root = !unused_scroll_delta.IsZero();
    scroll_result.unused_scroll_delta = unused_scroll_delta;

    controller_.ObserveGestureEventAndResult(event, scroll_result);
  }
  void SendGestureScrollEnd(base::TimeTicks time = base::TimeTicks::Now()) {
    WebGestureEvent event(WebInputEvent::Type::kGestureScrollEnd,
                          WebInputEvent::kNoModifiers, time,
                          WebGestureDevice::kTouchpad);

    controller_.ObserveGestureEventAndResult(event,
                                             cc::InputHandlerScrollResult());
  }

  MockScrollElasticityHelper helper_;
  ElasticOverscrollControllerBezier controller_;
};

// Tests that the scroller "stretches" as expected when an overscroll occurs
// on a non-scrollable area. See ReconcileStretchAndScroll for an overscroll
// stretch on scrollable areas.
TEST_F(ElasticOverscrollControllerBezierTest, OverscrollStretch) {
  // Test vertical overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
#else
  EXPECT_EQ(Vector2dF(0, -19), helper_.StretchAmount());
#endif
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, 100));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollEnd();

  // Test horizontal overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-100, 0));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
#else
  EXPECT_EQ(Vector2dF(-19, 0), helper_.StretchAmount());
#endif
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(100, 0));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollEnd();
}

// Verify that synthetic gesture events do not trigger an overscroll.
TEST_F(ElasticOverscrollControllerBezierTest, NoSyntheticEventsOverscroll) {
  // Test vertical overscroll.
  WebGestureEvent event(WebInputEvent::Type::kGestureScrollBegin,
                        WebInputEvent::kNoModifiers, base::TimeTicks(),
                        WebGestureDevice::kScrollbar);
  event.data.scroll_begin.inertial_phase = PhaseState::kNonMomentum;
  event.data.scroll_begin.synthetic = true;
  controller_.ObserveGestureEventAndResult(event,
                                           cc::InputHandlerScrollResult());
  EXPECT_EQ(controller_.state_,
            ElasticOverscrollController::State::kStateInactive);
}

// Verify that ReconcileStretchAndScroll reduces the overscrolled delta.
TEST_F(ElasticOverscrollControllerBezierTest, ReconcileStretchAndScroll) {
  // Test vertical overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));
  EXPECT_EQ(Vector2dF(0, -19), helper_.StretchAmount());
  helper_.ScrollBy(Vector2dF(0, 1));
  controller_.ReconcileStretchAndScroll();
  EXPECT_EQ(Vector2dF(0, -18), helper_.StretchAmount());

  // Reset vertical overscroll.
  helper_.SetStretchAmount(Vector2dF(0, 0));
  SendGestureScrollEnd(base::TimeTicks::Now());

  // Test horizontal overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-100, 0));
  EXPECT_EQ(Vector2dF(-19, 0), helper_.StretchAmount());
  helper_.ScrollBy(Vector2dF(1, 0));
  controller_.ReconcileStretchAndScroll();
  EXPECT_EQ(Vector2dF(-18, 0), helper_.StretchAmount());
}

// Tests that momentum_animation_start_time_ doesn't get reset when the
// overscroll animation is ticking and the scroller is diagonally overscrolled.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyInitialStretchDelta) {
  // Set up the state to be in kStateMomentumAnimated with some amount of
  // diagonal stretch.
  controller_.state_ =
      ElasticOverscrollController::State::kStateMomentumAnimated;
  helper_.SetStretchAmount(Vector2dF(5, 10));
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 20),
                                            gfx::PointF(100, 100));
  controller_.ReconcileStretchAndScroll();
  controller_.bounce_forwards_duration_x_ = base::Milliseconds(1000);
  controller_.bounce_forwards_duration_y_ = base::Milliseconds(1000);
  controller_.momentum_animation_initial_stretch_ = gfx::Vector2dF(10.f, 10.f);

  // Verify that the momentum_animation_start_time_ doesn't get reset when the
  // animation ticks.
  const base::TimeTicks animation_start_time =
      base::TimeTicks() + base::Milliseconds(32);

  // After 2 frames.
  controller_.Animate(animation_start_time);
  helper_.ScrollBy(Vector2dF(0, 2));
  EXPECT_NE(controller_.momentum_animation_start_time_, animation_start_time);
  EXPECT_EQ(controller_.state_,
            ElasticOverscrollController::State::kStateMomentumAnimated);

  // After 8 frames.
  controller_.Animate(animation_start_time + base::Milliseconds(128));
  helper_.ScrollBy(Vector2dF(0, 8));
  EXPECT_NE(controller_.momentum_animation_start_time_, animation_start_time);
  EXPECT_EQ(controller_.state_,
            ElasticOverscrollController::State::kStateMomentumAnimated);

  // After 64 frames the forward animation should no longer be active.
  controller_.Animate(animation_start_time + base::Milliseconds(1024));
  helper_.ScrollBy(Vector2dF(0, 64));
  EXPECT_NE(controller_.momentum_animation_start_time_, animation_start_time);
  EXPECT_EQ(controller_.state_,
            ElasticOverscrollController::State::kStateInactive);
  EXPECT_EQ(Vector2dF(), helper_.StretchAmount());
}

// Tests if the overscrolled delta maps correctly to the actual amount that the
// scroller gets stretched.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyOverscrollBounceDistance) {
  Vector2dF overscroll_bounce_distance(
      controller_.StretchAmountForAccumulatedOverscroll(Vector2dF(0, -100)));
  EXPECT_EQ(overscroll_bounce_distance.y(), -19);

  overscroll_bounce_distance =
      controller_.StretchAmountForAccumulatedOverscroll(Vector2dF(-100, 0));
  EXPECT_EQ(overscroll_bounce_distance.x(), -19);
}

// Tests that the bounce back animation ticks as expected. If the animation was
// successfully created, the call to OverscrollBounceController::Animate should
// tick the animation as expected. When the stretch amount is near 0, the
// scroller should treat the bounce as "completed".
TEST_F(ElasticOverscrollControllerBezierTest, VerifyBackwardAnimationTick) {
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));

  // Test vertical overscroll.
  EXPECT_EQ(controller_.state_, ElasticOverscrollController::kStateInactive);
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));

  // This signals that the finger has lifted off which triggers the bounce back
  // animation.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);

  // Frame 2.
  controller_.Animate(now + base::Milliseconds(32));
  EXPECT_EQ(controller_.state_,
            ElasticOverscrollController::kStateMomentumAnimated);
  ASSERT_FLOAT_EQ(helper_.StretchAmount().y(), -14);

  // Frame 5. The stretch amount moving closer to 0 proves that we're animating.
  controller_.Animate(now + base::Milliseconds(80));
  ASSERT_FLOAT_EQ(helper_.StretchAmount().y(), -8);

  // Frame 15. StretchAmount < abs(1), so snap to 0. state_ is kStateInactive.
  controller_.Animate(now + base::Milliseconds(240));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());

  EXPECT_EQ(controller_.state_, ElasticOverscrollController::kStateInactive);

  // Test horizontal overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-80, 0));
  SendGestureScrollEnd(now);

  // Frame 2.
  controller_.Animate(now + base::Milliseconds(32));
  ASSERT_FLOAT_EQ(helper_.StretchAmount().x(), -10);

  // Frame 5. The stretch amount moving closer to 0 proves that we're animating.
  controller_.Animate(now + base::Milliseconds(80));
  EXPECT_EQ(controller_.state_,
            ElasticOverscrollController::kStateMomentumAnimated);
  ASSERT_FLOAT_EQ(helper_.StretchAmount().x(), -5);

  // Frame 15. StretchAmount < abs(1), so snap to 0. state_ is kStateInactive.
  controller_.Animate(now + base::Milliseconds(240));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  EXPECT_EQ(controller_.state_, ElasticOverscrollController::kStateInactive);
}

// Tests that the bounce forward animation ticks as expected.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyForwardAnimationTick) {
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));

  // Test vertical forward bounce animations.
  EXPECT_EQ(controller_.state_, ElasticOverscrollController::kStateInactive);
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));
  controller_.scroll_velocity_ = gfx::Vector2dF(0.f, -4000.f);

  // This signals that the finger has lifted off which triggers a fling.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);

  const int TOTAL_FRAMES = 28;
  const int stretch_amount_y[TOTAL_FRAMES] = {
      -19, -41, -55, -65, -72, -78, -82, -85, -88, -89, -78, -64, -53, -44,
      -37, -30, -25, -20, -16, -13, -10, -7,  -5,  -4,  -2,  -1,  -1,  0};

  for (int i = 0; i < TOTAL_FRAMES; i++) {
    controller_.Animate(now + base::Milliseconds(i * 16));
    EXPECT_EQ(controller_.state_,
              (stretch_amount_y[i] == 0
                   ? ElasticOverscrollController::kStateInactive
                   : ElasticOverscrollController::kStateMomentumAnimated));
    ASSERT_FLOAT_EQ(helper_.StretchAmount().y(), stretch_amount_y[i]);
  }

  // Test horizontal forward bounce animations.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-50, 0));
  controller_.scroll_velocity_ = gfx::Vector2dF(-3000.f, 0.f);
  SendGestureScrollEnd(now);

  const int stretch_amount_x[TOTAL_FRAMES] = {
      -9,  -24, -34, -42, -48, -54, -58, -62, -66, -69, -62, -52, -43, -36,
      -30, -25, -20, -17, -13, -10, -8,  -6,  -4,  -3,  -2,  -1,  0,   0};

  for (int i = 0; i < TOTAL_FRAMES; i++) {
    controller_.Animate(now + base::Milliseconds(i * 16));
    EXPECT_EQ(controller_.state_,
              (stretch_amount_x[i] == 0
                   ? ElasticOverscrollController::kStateInactive
                   : ElasticOverscrollController::kStateMomentumAnimated));
    ASSERT_FLOAT_EQ(helper_.StretchAmount().x(), stretch_amount_x[i]);
  }
}

// Tests that the bounce forward animation is *not* played when the velocity is
// less than kIgnoreForwardBounceVelocityThreshold. This can be verified by
// checking bounce_forwards_distance_ (since it is a function of velocity)
TEST_F(ElasticOverscrollControllerBezierTest,
       VerifyForwardAnimationIsNotPlayed) {
  EXPECT_EQ(Vector2dF(), helper_.StretchAmount());
  controller_.scroll_velocity_ = gfx::Vector2dF(0.f, -199.f);
  controller_.DidEnterMomentumAnimatedState();
  EXPECT_TRUE(controller_.bounce_forwards_distance_.IsZero());

  controller_.scroll_velocity_ = gfx::Vector2dF(-199.f, 0.f);
  controller_.DidEnterMomentumAnimatedState();
  EXPECT_TRUE(controller_.bounce_forwards_distance_.IsZero());

  // When velocity > 200, forward animation is expected to be played.
  controller_.scroll_velocity_ = gfx::Vector2dF(0.f, -201.f);
  controller_.DidEnterMomentumAnimatedState();
  EXPECT_EQ(gfx::Vector2dF(0, -16),
            gfx::ToRoundedVector2d(controller_.bounce_forwards_distance_));

  controller_.scroll_velocity_ = gfx::Vector2dF(-201.f, 0.f);
  controller_.DidEnterMomentumAnimatedState();
  EXPECT_EQ(gfx::Vector2dF(-16, 0),
            gfx::ToRoundedVector2d(controller_.bounce_forwards_distance_));
}

// Tests initiating a scroll when a bounce back animation is in progress works
// as expected.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyScrollDuringBounceBack) {
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));

  // Test vertical overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));

  // This signals that the finger has lifted off which triggers the bounce back
  // animation.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);
  EXPECT_EQ(Vector2dF(0, -19), helper_.StretchAmount());

  // Frame 2.
  controller_.Animate(now + base::Milliseconds(32));
  ASSERT_FLOAT_EQ(helper_.StretchAmount().y(), -14);

  // Frame 5. The stretch amount moving closer to 0 proves that we're animating.
  controller_.Animate(now + base::Milliseconds(80));
  ASSERT_FLOAT_EQ(helper_.StretchAmount().y(), -8);

  // While the animation is still ticking, initiate a scroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -50));
  ASSERT_FLOAT_EQ(helper_.StretchAmount().y(), -17);
}

// Tests that animation doesn't get created when unused_delta is 0.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyAnimationNotCreated) {
  // Test vertical and horizontal overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());

  // state_ is kStateActiveScroll. unused_delta is 0 so overscroll should not
  // take place.
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, 0));

  // This signals that the finger has lifted off which triggers the bounce back
  // animation.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);

  // Frame 2.
  controller_.Animate(now + base::Milliseconds(32));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount());
}
}  // namespace blink
