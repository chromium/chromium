// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_bezier.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

namespace {
// The following constants are determined experimentally.

// Used to determine how far the scroller is allowed to stretch.
constexpr double kOverscrollBoundaryMultiplier = 0.1f;

// Maximum duration for the bounce back animation.
constexpr double kBounceBackMaxDurationMilliseconds = 300.0;

// Time taken by the bounce back animation (in milliseconds) to scroll 1 px.
constexpr double kBounceBackMillisecondsPerPixel = 15.0;

// Threshold above which a forward animation should be played. Stray finger
// movements can cause velocities to be non-zero. This in-turn may lead to minor
// jerks when the bounce back animation is being played. Expressed in pixels per
// second.
constexpr double kIgnoreForwardBounceVelocityThreshold = 200;

constexpr double kOverbounceMaxDurationMilliseconds = 150.0;
constexpr double kOverbounceMillisecondsPerPixel = 2.5;
constexpr double kOverbounceDistanceMultiplier = 35.f;

// Control points for the bounce forward Cubic Bezier curve.
constexpr double kBounceForwardsX1 = 0.25;
constexpr double kBounceForwardsY1 = 1.0;
constexpr double kBounceForwardsX2 = 0.99;
constexpr double kBounceForwardsY2 = 1.0;

// Control points for the bounce back Cubic Bezier curve.
constexpr double kBounceBackwardsX1 = 0.05;
constexpr double kBounceBackwardsY1 = 0.7;
constexpr double kBounceBackwardsX2 = 0.25;
constexpr double kBounceBackwardsY2 = 1.0;

base::TimeDelta CalculateBounceForwardsDuration(
    double bounce_forwards_distance) {
  return base::Milliseconds(
      std::min(kOverbounceMaxDurationMilliseconds,
               kOverbounceMillisecondsPerPixel * bounce_forwards_distance));
}

base::TimeDelta CalculateBounceBackDuration(double bounce_back_distance) {
  return base::Milliseconds(std::min(
      kBounceBackMaxDurationMilliseconds,
      kBounceBackMillisecondsPerPixel * std::abs(bounce_back_distance)));
}
}  // namespace

// Scale one of the control points of the Cubic Bezier curve based on the
// initial_velocity (which is expressed in terms of pixels / ms).
gfx::CubicBezier InitialVelocityBasedBezierCurve(const double initial_velocity,
                                                 const double x1,
                                                 const double y1,
                                                 const double x2,
                                                 const double y2) {
  const double velocity = std::abs(initial_velocity);
  double x = x1, y = y1;
  if (x1 * velocity < y1) {
    y = x1 * velocity;
  } else {
    x = y1 / velocity;
  }

  return gfx::CubicBezier(x, y, x2, y2);
}

ElasticOverscrollControllerBezier::ElasticOverscrollControllerBezier(
    cc::ScrollElasticityHelper* helper)
    : ElasticOverscrollController(helper) {}

// Returns the maximum amount to be overscrolled.
gfx::Vector2dF ElasticOverscrollControllerBezier::OverscrollBoundary(
    const gfx::Size& scroller_bounds) const {
  return gfx::Vector2dF(
      scroller_bounds.width() * kOverscrollBoundaryMultiplier,
      scroller_bounds.height() * kOverscrollBoundaryMultiplier);
}

void ElasticOverscrollControllerBezier::DidEnterMomentumAnimatedState() {
  // Express velocity in terms of milliseconds.
  const gfx::Vector2dF velocity(
      fabs(scroll_velocity().x()) > kIgnoreForwardBounceVelocityThreshold
          ? scroll_velocity().x() / 1000.f
          : 0.f,
      fabs(scroll_velocity().y()) > kIgnoreForwardBounceVelocityThreshold
          ? scroll_velocity().y() / 1000.f
          : 0.f);

  residual_velocity_ = velocity;

  gfx::Vector2dF bounce_forwards_delta(gfx::Vector2dF(
      sqrt(std::abs(velocity.x())), sqrt(std::abs(velocity.y()))));
  bounce_forwards_delta.Scale(kOverbounceDistanceMultiplier);

  const gfx::Vector2dF max_stretch_amount = OverscrollBoundary(scroll_bounds());
  bounce_forwards_distance_.set_x(
      std::min(max_stretch_amount.x(),
               std::abs(momentum_animation_initial_stretch_.x()) +
                   bounce_forwards_delta.x()));
  bounce_forwards_distance_.set_y(
      std::min(max_stretch_amount.y(),
               std::abs(momentum_animation_initial_stretch_.y()) +
                   bounce_forwards_delta.y()));

  // If we're flinging towards the edge, the sign of the distance will match
  // that of the velocity. Otherwise, it will match that of the current
  // stretch amount.
  bounce_forwards_distance_.set_x(
      (momentum_animation_initial_stretch_.x() == 0)
          ? std::copysign(bounce_forwards_distance_.x(), velocity.x())
          : std::copysign(bounce_forwards_distance_.x(),
                          momentum_animation_initial_stretch_.x()));
  bounce_forwards_distance_.set_y(
      (momentum_animation_initial_stretch_.y() == 0)
          ? std::copysign(bounce_forwards_distance_.y(), velocity.y())
          : std::copysign(bounce_forwards_distance_.y(),
                          momentum_animation_initial_stretch_.y()));
  bounce_forwards_duration_x_ =
      CalculateBounceForwardsDuration(bounce_forwards_delta.x());
  bounce_forwards_duration_y_ =
      CalculateBounceForwardsDuration(bounce_forwards_delta.y());

  bounce_backwards_duration_x_ =
      CalculateBounceBackDuration(bounce_forwards_distance_.x());
  bounce_backwards_duration_y_ =
      CalculateBounceBackDuration(bounce_forwards_distance_.y());
}

double ElasticOverscrollControllerBezier::StretchAmountForForwardBounce(
    const gfx::CubicBezier bounce_forwards_curve,
    const base::TimeDelta& delta,
    const base::TimeDelta& bounce_forwards_duration,
    const double velocity,
    const double initial_stretch,
    const double bounce_forwards_distance) const {
  const bool is_velocity_in_overscroll_direction =
      (velocity < 0) == (initial_stretch < 0);
  if (is_velocity_in_overscroll_direction) {
    if (delta < bounce_forwards_duration) {
      double curve_progress =
          delta.InMillisecondsF() / bounce_forwards_duration.InMillisecondsF();
      double progress = bounce_forwards_curve.Solve(curve_progress);
      return initial_stretch * (1 - progress) +
             bounce_forwards_distance * progress;
    }
  }
  return 0.f;
}

double ElasticOverscrollControllerBezier::StretchAmountForBackwardBounce(
    const gfx::CubicBezier bounce_backwards_curve,
    const base::TimeDelta& delta,
    const base::TimeDelta& bounce_backwards_duration,
    const double bounce_forwards_distance) const {
  if (delta < bounce_backwards_duration) {
    double curve_progress =
        delta.InMillisecondsF() / bounce_backwards_duration.InMillisecondsF();
    double progress = bounce_backwards_curve.Solve(curve_progress);
    return bounce_forwards_distance * (1 - progress);
  }
  return 0.f;
}

gfx::Vector2d ElasticOverscrollControllerBezier::StretchAmountForTimeDelta(
    const base::TimeDelta& delta) const {
  // Check if a bounce forward animation needs to be created. This is needed
  // when user "flings" a scroller. By the time the scroller reaches its bounds,
  // if the velocity isn't 0, a bounce forwards animation will need to be
  // played.
  base::TimeDelta time_delta = delta;
  const gfx::CubicBezier bounce_forwards_curve_x =
      InitialVelocityBasedBezierCurve(residual_velocity_.x(), kBounceForwardsX1,
                                      kBounceForwardsY1, kBounceForwardsX2,
                                      kBounceForwardsY2);
  const gfx::CubicBezier bounce_forwards_curve_y =
      InitialVelocityBasedBezierCurve(residual_velocity_.y(), kBounceForwardsX1,
                                      kBounceForwardsY1, kBounceForwardsX2,
                                      kBounceForwardsY2);
  const gfx::Vector2d forward_animation(gfx::ToRoundedVector2d(gfx::Vector2dF(
      StretchAmountForForwardBounce(
          bounce_forwards_curve_x, time_delta, bounce_forwards_duration_x_,
          scroll_velocity().x(), momentum_animation_initial_stretch_.x(),
          bounce_forwards_distance_.x()),
      StretchAmountForForwardBounce(
          bounce_forwards_curve_y, time_delta, bounce_forwards_duration_y_,
          scroll_velocity().y(), momentum_animation_initial_stretch_.y(),
          bounce_forwards_distance_.y()))));

  if (!forward_animation.IsZero())
    return forward_animation;

  // Handle the case where the animation is in the bounce-back stage.
  time_delta -= bounce_forwards_duration_x_;
  time_delta -= bounce_forwards_duration_y_;

  const gfx::CubicBezier bounce_backwards_curve_x =
      InitialVelocityBasedBezierCurve(residual_velocity_.x(),
                                      kBounceBackwardsX1, kBounceBackwardsY1,
                                      kBounceBackwardsX2, kBounceBackwardsY2);
  const gfx::CubicBezier bounce_backwards_curve_y =
      InitialVelocityBasedBezierCurve(residual_velocity_.y(),
                                      kBounceBackwardsX1, kBounceBackwardsY1,
                                      kBounceBackwardsX2, kBounceBackwardsY2);
  return gfx::ToRoundedVector2d(gfx::Vector2dF(
      StretchAmountForBackwardBounce(bounce_backwards_curve_x, time_delta,
                                     bounce_backwards_duration_x_,
                                     bounce_forwards_distance_.x()),
      StretchAmountForBackwardBounce(bounce_backwards_curve_y, time_delta,
                                     bounce_backwards_duration_y_,
                                     bounce_forwards_distance_.y())));
}

// The goal of this calculation is to map the distance the user has scrolled
// past the boundary into the distance to actually scroll the elastic scroller.
gfx::Vector2d
ElasticOverscrollControllerBezier::StretchAmountForAccumulatedOverscroll(
    const gfx::Vector2dF& accumulated_overscroll) const {
  // TODO(arakeri): This should change as you pinch zoom in.
  const gfx::Vector2dF overscroll_boundary =
      OverscrollBoundary(scroll_bounds());

  // We use the tanh function in addition to the mapping, which gives it more of
  // a spring effect. However, we want to use tanh's range from [0, 2], so we
  // multiply the value we provide to tanh by 2.

  // Also, it may happen that the scroll_bounds are 0 if the viewport scroll
  // nodes are null (see: ScrollElasticityHelper::ScrollBounds). We therefore
  // have to check in order to avoid a divide by 0.
  gfx::Vector2d overbounce_distance;
  if (scroll_bounds().width() > 0.f) {
    overbounce_distance.set_x(
        tanh(2 * accumulated_overscroll.x() / scroll_bounds().width()) *
        overscroll_boundary.x());
  }

  if (scroll_bounds().height() > 0.f) {
    overbounce_distance.set_y(
        tanh(2 * accumulated_overscroll.y() / scroll_bounds().height()) *
        overscroll_boundary.y());
  }

  return overbounce_distance;
}

// This function does the inverse of StretchAmountForAccumulatedOverscroll. As
// in, instead of taking in the amount of distance overscrolled to get the
// bounce distance, it takes in the bounce distance and calculates how much is
// actually overscrolled.
gfx::Vector2d
ElasticOverscrollControllerBezier::AccumulatedOverscrollForStretchAmount(
    const gfx::Vector2dF& stretch_amount) const {
  const gfx::Vector2dF overscroll_boundary =
      OverscrollBoundary(scroll_bounds());

  // It may happen that the scroll_bounds are 0 if the viewport scroll
  // nodes are null (see: ScrollElasticityHelper::ScrollBounds). We therefore
  // have to check in order to avoid a divide by 0.
  gfx::Vector2d overscrolled_amount;
  if (overscroll_boundary.x() > 0.f) {
    float atanh_value = atanh(stretch_amount.x() / overscroll_boundary.x());
    overscrolled_amount.set_x((atanh_value / 2) * scroll_bounds().width());
  }

  if (overscroll_boundary.y() > 0.f) {
    float atanh_value = atanh(stretch_amount.y() / overscroll_boundary.y());
    overscrolled_amount.set_y((atanh_value / 2) * scroll_bounds().height());
  }

  return overscrolled_amount;
}
}  // namespace blink
