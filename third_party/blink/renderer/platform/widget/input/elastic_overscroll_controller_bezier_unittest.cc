// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_bezier.h"

#include <array>

#include "build/build_config.h"
#include "cc/input/input_handler.h"
#include "cc/paint/element_id.h"
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
  Size ScrollBounds(cc::ElementId element_id) const override {
    return Size(1000, 1000);
  }
  Vector2dF ConstrainOverscrollDelta(cc::ElementId element_id,
                                     const Vector2dF& delta) const override {
    return delta;
  }
  bool IsUserOverscrollable(cc::ElementId element_id) const override {
    return true;
  }
  Vector2dF StretchAmount(cc::ElementId element_id) const override {
    return stretch_amount_;
  }
  void SetStretchAmount(cc::ElementId element_id,
                        const Vector2dF& stretch_amount) override {
    stretch_amount_ = stretch_amount;
  }
  void ResetStretchAmounts() override { stretch_amount_ = gfx::Vector2dF(); }
  void ApplyStretchAmountsToPending() override {}
  void ApplyStretchAmountsToActive() override {}
  void ScrollBy(cc::ElementId element_id, const Vector2dF& delta) override {
    scroll_offset_ += delta;
  }
  void RequestOneBeginFrame() override {}
  void AnimationFinished(cc::ElementId element_id) override {}
  gfx::PointF ScrollOffset(cc::ElementId element_id) const override {
    return scroll_offset_;
  }
  gfx::PointF MaxScrollOffset(cc::ElementId element_id) const override {
    return max_scroll_offset_;
  }

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

    controller_.ObserveGestureEventAndResult(cc::ElementId(), event,
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

    controller_.ObserveGestureEventAndResult(cc::ElementId(), event,
                                             scroll_result);
  }
  void SendGestureScrollEnd(base::TimeTicks time = base::TimeTicks::Now()) {
    WebGestureEvent event(WebInputEvent::Type::kGestureScrollEnd,
                          WebInputEvent::kNoModifiers, time,
                          WebGestureDevice::kTouchpad);

    controller_.ObserveGestureEventAndResult(cc::ElementId(), event,
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
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
#else
  EXPECT_EQ(Vector2dF(0, -19), helper_.StretchAmount(cc::ElementId()));
#endif
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, 100));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  SendGestureScrollEnd();

  // Test horizontal overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-100, 0));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
#else
  EXPECT_EQ(Vector2dF(-19, 0), helper_.StretchAmount(cc::ElementId()));
#endif
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(100, 0));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
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
  controller_.ObserveGestureEventAndResult(cc::ElementId(), event,
                                           cc::InputHandlerScrollResult());
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::State::kStateInactive);
}

// Verify that ReconcileStretchAndScroll reduces the overscrolled delta.
TEST_F(ElasticOverscrollControllerBezierTest, ReconcileStretchAndScroll) {
  // Test vertical overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));
  EXPECT_EQ(Vector2dF(0, -19), helper_.StretchAmount(cc::ElementId()));
  helper_.ScrollBy(cc::ElementId(), Vector2dF(0, 1));
  controller_.ReconcileStretchAndScroll();
  EXPECT_EQ(Vector2dF(0, -18), helper_.StretchAmount(cc::ElementId()));

  // Reset vertical overscroll.
  helper_.SetStretchAmount(cc::ElementId(), Vector2dF(0, 0));
  SendGestureScrollEnd(base::TimeTicks::Now());

  // Test horizontal overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-100, 0));
  EXPECT_EQ(Vector2dF(-19, 0), helper_.StretchAmount(cc::ElementId()));
  helper_.ScrollBy(cc::ElementId(), Vector2dF(1, 0));
  controller_.ReconcileStretchAndScroll();
  EXPECT_EQ(Vector2dF(-18, 0), helper_.StretchAmount(cc::ElementId()));
}

// Tests that momentum_animation_start_time_ doesn't get reset when the
// overscroll animation is ticking and the scroller is diagonally overscrolled.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyInitialStretchDelta) {
  auto to_bezier = [](auto&& entry) -> decltype(auto) {
    return static_cast<
        ElasticOverscrollControllerBezier::BezierOverscrollEntry&>(entry);
  };
  // Set up the state to be in kStateMomentumAnimated with some amount of
  // diagonal stretch.
  controller_.EnsureEntry(cc::ElementId()).state =
      ElasticOverscrollController::State::kStateMomentumAnimated;
  helper_.SetStretchAmount(cc::ElementId(), Vector2dF(5, 10));
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 20),
                                            gfx::PointF(100, 100));
  controller_.ReconcileStretchAndScroll();
  to_bezier(controller_.EnsureEntry(cc::ElementId()))
      .bounce_forwards_duration_x = base::Milliseconds(1000);
  to_bezier(controller_.EnsureEntry(cc::ElementId()))
      .bounce_forwards_duration_y = base::Milliseconds(1000);
  to_bezier(controller_.EnsureEntry(cc::ElementId()))
      .momentum_animation_initial_stretch = gfx::Vector2dF(10.f, 10.f);

  // Verify that the momentum_animation_start_time_ doesn't get reset when the
  // animation ticks.
  const base::TimeTicks animation_start_time =
      base::TimeTicks() + base::Milliseconds(32);

  // After 2 frames.
  controller_.Animate(animation_start_time);
  helper_.ScrollBy(cc::ElementId(), Vector2dF(0, 2));
  EXPECT_NE(
      controller_.EnsureEntry(cc::ElementId()).momentum_animation_start_time,
      animation_start_time);
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::State::kStateMomentumAnimated);

  // After 8 frames.
  controller_.Animate(animation_start_time + base::Milliseconds(128));
  helper_.ScrollBy(cc::ElementId(), Vector2dF(0, 8));
  EXPECT_NE(
      controller_.EnsureEntry(cc::ElementId()).momentum_animation_start_time,
      animation_start_time);
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::State::kStateMomentumAnimated);

  // After 64 frames the forward animation should no longer be active.
  controller_.Animate(animation_start_time + base::Milliseconds(1024));
  helper_.ScrollBy(cc::ElementId(), Vector2dF(0, 64));
  EXPECT_NE(
      controller_.EnsureEntry(cc::ElementId()).momentum_animation_start_time,
      animation_start_time);
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::State::kStateInactive);
  EXPECT_EQ(Vector2dF(), helper_.StretchAmount(cc::ElementId()));
}

// Tests if the overscrolled delta maps correctly to the actual amount that the
// scroller gets stretched.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyOverscrollBounceDistance) {
  const ElasticOverscrollController::OverscrollEntry entry{cc::ElementId()};
  Vector2dF overscroll_bounce_distance(
      controller_.StretchAmountForAccumulatedOverscroll(entry,
                                                        Vector2dF(0, -100)));
  EXPECT_EQ(overscroll_bounce_distance.y(), -19);

  overscroll_bounce_distance =
      controller_.StretchAmountForAccumulatedOverscroll(entry,
                                                        Vector2dF(-100, 0));
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
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::kStateInactive);
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));

  // This signals that the finger has lifted off which triggers the bounce back
  // animation.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);

  // Frame 2.
  controller_.Animate(now + base::Milliseconds(32));
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::kStateMomentumAnimated);
  ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).y(), -14);

  // Frame 5. The stretch amount moving closer to 0 proves that we're animating.
  controller_.Animate(now + base::Milliseconds(80));
  ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).y(), -8);

  // Frame 15. StretchAmount < abs(1), so snap to 0. state_ is kStateInactive.
  controller_.Animate(now + base::Milliseconds(240));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));

  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::kStateInactive);

  // Test horizontal overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-80, 0));
  SendGestureScrollEnd(now);

  // Frame 2.
  controller_.Animate(now + base::Milliseconds(32));
  ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).x(), -10);

  // Frame 5. The stretch amount moving closer to 0 proves that we're animating.
  controller_.Animate(now + base::Milliseconds(80));
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::kStateMomentumAnimated);
  ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).x(), -5);

  // Frame 15. StretchAmount < abs(1), so snap to 0. state_ is kStateInactive.
  controller_.Animate(now + base::Milliseconds(240));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::kStateInactive);
}

// Tests that the bounce forward animation ticks as expected.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyForwardAnimationTick) {
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));

  // Test vertical forward bounce animations.
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::kStateInactive);
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));
  controller_.EnsureEntry(cc::ElementId()).scroll_velocity =
      gfx::Vector2dF(0.f, -4000.f);

  // This signals that the finger has lifted off which triggers a fling.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);

  const int TOTAL_FRAMES = 28;
  const std::array<int, TOTAL_FRAMES> stretch_amount_y = {
      -19, -41, -55, -65, -72, -78, -82, -85, -88, -89, -78, -64, -53, -44,
      -37, -30, -25, -20, -16, -13, -10, -7,  -5,  -4,  -2,  -1,  -1,  0,
  };

  for (int i = 0; i < TOTAL_FRAMES; i++) {
    controller_.Animate(now + base::Milliseconds(i * 16));
    EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
              (stretch_amount_y[i] == 0
                   ? ElasticOverscrollController::kStateInactive
                   : ElasticOverscrollController::kStateMomentumAnimated));
    ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).y(),
                    stretch_amount_y[i]);
  }

  // Test horizontal forward bounce animations.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-50, 0));
  controller_.EnsureEntry(cc::ElementId()).scroll_velocity =
      gfx::Vector2dF(-3000.f, 0.f);
  SendGestureScrollEnd(now);

  const std::array<int, TOTAL_FRAMES> stretch_amount_x = {
      -9,  -24, -34, -42, -48, -54, -58, -62, -66, -69, -62, -52, -43, -36,
      -30, -25, -20, -17, -13, -10, -8,  -6,  -4,  -3,  -2,  -1,  0,   0,
  };

  for (int i = 0; i < TOTAL_FRAMES; i++) {
    controller_.Animate(now + base::Milliseconds(i * 16));
    EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
              (stretch_amount_x[i] == 0
                   ? ElasticOverscrollController::kStateInactive
                   : ElasticOverscrollController::kStateMomentumAnimated));
    ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).x(),
                    stretch_amount_x[i]);
  }
}

// Tests that the bounce forward animation is *not* played when the velocity is
// less than kIgnoreForwardBounceVelocityThreshold. This can be verified by
// checking bounce_forwards_distance_ (since it is a function of velocity)
TEST_F(ElasticOverscrollControllerBezierTest,
       VerifyForwardAnimationIsNotPlayed) {
  auto to_bezier = [](auto&& entry) -> decltype(auto) {
    return static_cast<
        ElasticOverscrollControllerBezier::BezierOverscrollEntry&>(entry);
  };
  EXPECT_EQ(Vector2dF(), helper_.StretchAmount(cc::ElementId()));
  controller_.EnsureEntry(cc::ElementId()).scroll_velocity =
      gfx::Vector2dF(0.f, -199.f);
  controller_.DidEnterMomentumAnimatedState(
      controller_.EnsureEntry(cc::ElementId()));
  EXPECT_TRUE(to_bezier(controller_.EnsureEntry(cc::ElementId()))
                  .bounce_forwards_distance.IsZero());

  controller_.EnsureEntry(cc::ElementId()).scroll_velocity =
      gfx::Vector2dF(-199.f, 0.f);
  controller_.DidEnterMomentumAnimatedState(
      controller_.EnsureEntry(cc::ElementId()));
  EXPECT_TRUE(to_bezier(controller_.EnsureEntry(cc::ElementId()))
                  .bounce_forwards_distance.IsZero());

  // When velocity > 200, forward animation is expected to be played.
  controller_.EnsureEntry(cc::ElementId()).scroll_velocity =
      gfx::Vector2dF(0.f, -201.f);
  controller_.DidEnterMomentumAnimatedState(
      controller_.EnsureEntry(cc::ElementId()));
  EXPECT_EQ(
      gfx::Vector2dF(0, -16),
      gfx::ToRoundedVector2d(to_bezier(controller_.EnsureEntry(cc::ElementId()))
                                 .bounce_forwards_distance));

  controller_.EnsureEntry(cc::ElementId()).scroll_velocity =
      gfx::Vector2dF(-201.f, 0.f);
  controller_.DidEnterMomentumAnimatedState(
      controller_.EnsureEntry(cc::ElementId()));
  EXPECT_EQ(
      gfx::Vector2dF(-16, 0),
      gfx::ToRoundedVector2d(to_bezier(controller_.EnsureEntry(cc::ElementId()))
                                 .bounce_forwards_distance));
}

// Tests initiating a scroll when a bounce back animation is in progress works
// as expected.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyScrollDuringBounceBack) {
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));

  // Test vertical overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -100));

  // This signals that the finger has lifted off which triggers the bounce back
  // animation.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);
  EXPECT_EQ(Vector2dF(0, -19), helper_.StretchAmount(cc::ElementId()));

  // Frame 2.
  controller_.Animate(now + base::Milliseconds(32));
  ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).y(), -14);

  // Frame 5. The stretch amount moving closer to 0 proves that we're animating.
  controller_.Animate(now + base::Milliseconds(80));
  ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).y(), -8);

  // While the animation is still ticking, initiate a scroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, -50));
  ASSERT_FLOAT_EQ(helper_.StretchAmount(cc::ElementId()).y(), -17);
}

// Tests that animation doesn't get created when unused_delta is 0.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyAnimationNotCreated) {
  // Test vertical and horizontal overscroll.
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));

  // state_ is kStateActiveScroll. unused_delta is 0 so overscroll should not
  // take place.
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(0, 0));

  // This signals that the finger has lifted off which triggers the bounce back
  // animation.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);

  // Frame 2.
  controller_.Animate(now + base::Milliseconds(32));
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
}

// Tests that the forward bounce animation handles different animations in two
// axis with different durations gracefully.
TEST_F(ElasticOverscrollControllerBezierTest,
       VerifyDifferentDurationForwardAnimations) {
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));

  // Test vertical forward bounce animations.
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::kStateInactive);
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  // The gesture will be much greater vertically than horizontally. This should
  // cause the animation to be longer on the Y axis than on the X axis.
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-50, -100));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-50, 0));
  // Verify that both axis are stretched before the fling gesture.
  EXPECT_GT(fabsf(helper_.StretchAmount(cc::ElementId()).x()), 0);
  EXPECT_GT(fabsf(helper_.StretchAmount(cc::ElementId()).y()), 0);

  controller_.EnsureEntry(cc::ElementId()).scroll_velocity =
      gfx::Vector2dF(-1000.f, -4000.f);

  // This signals that the finger has lifted off which triggers a fling.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);

  constexpr int kMaxFrames = 100;
  controller_.Animate(now);
  float x_stretch_amount = fabsf(helper_.StretchAmount(cc::ElementId()).x());
  float y_stretch_amount = fabsf(helper_.StretchAmount(cc::ElementId()).y());
  enum AnimationState {
    kBouncingForwardBoth,
    kBouncingForwardY,
    kBouncingBackwards
  };
  AnimationState state(kBouncingForwardBoth);
  for (int i = 1;
       i < kMaxFrames && (x_stretch_amount > 0 || y_stretch_amount > 0); i++) {
    controller_.Animate(now + base::Milliseconds(i * 16));
    const float new_x_stretch_amount =
        fabs(helper_.StretchAmount(cc::ElementId()).x());
    const float new_y_stretch_amount =
        fabs(helper_.StretchAmount(cc::ElementId()).y());
    if (state == kBouncingForwardBoth &&
        new_x_stretch_amount == x_stretch_amount) {
      EXPECT_NE(new_x_stretch_amount, 0);
      state = kBouncingForwardY;
    }
    if (state == kBouncingForwardY &&
        new_y_stretch_amount <= y_stretch_amount) {
      state = kBouncingBackwards;
    }
    switch (state) {
      case kBouncingForwardBoth:
        // While both axis are bouncing forward, the stretch amount should
        // increase on each tick of the animation.
        EXPECT_GT(new_x_stretch_amount, x_stretch_amount);
        EXPECT_GT(new_y_stretch_amount, y_stretch_amount);
        break;
      case kBouncingForwardY:
        // While one axis has completed it's animation and the other one hasn't,
        // only the one still animating should increase in value.
        EXPECT_EQ(new_x_stretch_amount, x_stretch_amount);
        EXPECT_GT(new_y_stretch_amount, y_stretch_amount);
        break;
      case kBouncingBackwards:
        // Once the bounce backwards animation has kicked in, both stretches
        // should monotonically decrease until they become zero.
        EXPECT_LE(new_x_stretch_amount, x_stretch_amount);
        EXPECT_LE(new_y_stretch_amount, y_stretch_amount);
        break;
    }
    y_stretch_amount = new_y_stretch_amount;
    x_stretch_amount = new_x_stretch_amount;
  }
  // Verify that the loop ended because the animation did and not because we hit
  // the max amount of frames.
  EXPECT_FLOAT_EQ(x_stretch_amount, 0.f);
  EXPECT_FLOAT_EQ(y_stretch_amount, 0.f);
}

// Tests that the forward bounce animation handles single axis animations
// gracefully.
TEST_F(ElasticOverscrollControllerBezierTest, VerifyOneAxisForwardAnimation) {
  helper_.SetScrollOffsetAndMaxScrollOffset(gfx::PointF(0, 0),
                                            gfx::PointF(100, 100));

  // Test vertical forward bounce animations.
  EXPECT_EQ(controller_.EnsureEntry(cc::ElementId()).state,
            ElasticOverscrollController::kStateInactive);
  SendGestureScrollBegin(PhaseState::kNonMomentum);
  EXPECT_EQ(Vector2dF(0, 0), helper_.StretchAmount(cc::ElementId()));
  // The gesture will be much greater vertically than horizontally. This should
  // cause the animation to be longer on the Y axis than on the X axis.
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-50, -100));
  SendGestureScrollUpdate(PhaseState::kNonMomentum, Vector2dF(-50, 0));
  // The X axis should be stretched out to verify that the animation doesn't
  // reset its value.
  EXPECT_GT(fabsf(helper_.StretchAmount(cc::ElementId()).x()), 0);

  controller_.EnsureEntry(cc::ElementId()).scroll_velocity =
      gfx::Vector2dF(0, -4000.f);

  // This signals that the finger has lifted off which triggers a fling.
  const base::TimeTicks now = base::TimeTicks::Now();
  SendGestureScrollEnd(now);

  constexpr int kMaxFrames = 100;
  controller_.Animate(now);
  float x_stretch_amount = fabsf(helper_.StretchAmount(cc::ElementId()).x());
  float y_stretch_amount = fabsf(helper_.StretchAmount(cc::ElementId()).y());
  // Animate the entire forward animation verifying that the x-axis doesn't get
  // moved.
  for (int i = 1; i < kMaxFrames && x_stretch_amount > 0; i++) {
    controller_.Animate(now + base::Milliseconds(i * 16));
    const float new_x_stretch_amount =
        fabs(helper_.StretchAmount(cc::ElementId()).x());
    const float new_y_stretch_amount =
        fabs(helper_.StretchAmount(cc::ElementId()).y());
    // Exit the loop when the forward animation ends.
    if (new_y_stretch_amount <= y_stretch_amount) {
      break;
    }

    EXPECT_FLOAT_EQ(new_x_stretch_amount, x_stretch_amount);
    y_stretch_amount = new_y_stretch_amount;
    x_stretch_amount = new_x_stretch_amount;
  }
}
}  // namespace blink
