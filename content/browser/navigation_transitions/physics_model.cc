// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/physics_model.h"

#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/math_constants.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"

// TODO(liuwilliam): The velocity and positions should have the same direction.
//
// Notes:
// - Directions: for offsets/positions, the right edge direction is "+" and the
//   left is "-"; for velocities, the right edge direction is "-" and the left
//   is "+".
// - The physics model internally operates in the normalized viewport space
//   while takes/returns physical pixel values as input/output. The spacial
//   variables are suffixed with `_viewport` or `_physical` to avoid confusion.

namespace content {

namespace {

// The tolerance value for which two floats are consider equal.
constexpr float kFloatTolerance = 0.001f;

// Used to replace NaN and Inf.
constexpr float kInvalidVelocity = 999.f;

// Springs.
//
// Determines when the spring is stabilized (the damped amplitude no longer
// changes significantly). Larger the value, the longer the spring takes to
// stabilize, but the spring amplitude is damped more gently.
constexpr int kSpringResponse = 708;

// How much the spring overshoots. Smaller the value, more bouncy the spring.
constexpr float kSpringDampingRatio = 0.81f;

// The size of the spring location history.
constexpr int kSpringHistorySize = 10;

// A spring is considered at rest if it has used at least
// `kSpringAtRestThreshold`*`kSpringHistorySize` amount of energy.
constexpr int kSpringAtRestThreshold = 10;

// Physics model.
//
// The live page of the current content will stop at 85% of the screen width
// while wait for the navigation to the new page to commit.
constexpr float kTargetCommitPending = 0.85f;

// The size of the touch points store in `PhysicsModel`. Used to interpolate
// the finger's terminal velocity when the model switches from the finger drag
// curve driven to spring driven.
constexpr int kPhysicsModelHistorySize = 10;

bool IsInvalidVelocity(float velocity) {
  return base::IsApproximatelyEqual(velocity, kInvalidVelocity,
                                    kFloatTolerance);
}

// Solves `positions`=`slope`*`timestamps`+ displacement(not calculated).
//
// TODO(https://crbug.com/1504838): The native least square might not give us
// the desired velocity.
void SolveLeastSquare(const std::vector<float>& timestamps,
                      const std::vector<float>& positions,
                      float* slope) {
  CHECK_EQ(timestamps.size(), positions.size());

  const size_t num_pts = timestamps.size();
  if (num_pts <= 1) {
    LOG(ERROR) << "Interpolating velocity with " << num_pts << " points";
    if (slope) {
      *slope = kInvalidVelocity;
    }
    return;
  }

  float sum_timestamps = 0;
  float sum_positions = 0;
  float sum_times_positions = 0;
  float sum_timestamps_sq = 0;

  for (size_t i = 0; i < num_pts; ++i) {
    float t = timestamps[i];
    float p = positions[i];
    sum_timestamps += t;
    sum_positions += p;
    sum_times_positions += t * p;
    sum_timestamps_sq += t * t;
  }

  if (slope) {
    float denominator =
        (sum_timestamps_sq - sum_timestamps * sum_timestamps / num_pts);
    if (base::IsApproximatelyEqual(denominator, 0.f, kFloatTolerance)) {
      *slope = kInvalidVelocity;
    } else {
      *slope =
          (sum_times_positions - sum_timestamps * sum_positions / num_pts) /
          denominator;
    }
  }
}

}  // namespace

class Spring {
 public:
  struct Position {
    // Calculated offset of the spring's position w.r.t. its equilibrium.
    float equilibrium_offset_viewport;

    // The amount of time delta since the spring is released (i.e., the start of
    // the animation).
    base::TimeDelta timestamp;

    // If the spring is at rest then it won't bounce anymore. A spring is at
    // rest if it has lost enough energe, or it is <= 1 pixel away from its
    // equilibrium.
    bool at_rest;
  };

  Spring(int frequency_response,
         float damping_ratio,
         float device_scaling_factor)
      : frequency_response_(frequency_response),
        damping_ratio_(damping_ratio),
        device_scale_factor_(device_scaling_factor) {
    float stiffness =
        std::pow(2 * base::kPiFloat / frequency_response_, 2) * mass_;
    undamped_natural_frequency_ = std::sqrt(stiffness / mass_);
    damped_natural_frequency_ =
        undamped_natural_frequency_ *
        std::sqrt(std::abs(1 - std::pow(damping_ratio_, 2)));
    // `damped_natural_frequency_` will be used as a denominator. It shouldn't
    // be zero.
    CHECK(!base::IsApproximatelyEqual(damped_natural_frequency_, 0.f,
                                      kFloatTolerance));
    CHECK(!base::IsApproximatelyEqual(device_scale_factor_, 0.f,
                                      kFloatTolerance));
  }
  Spring(const Spring&) = delete;
  Spring& operator=(const Spring&) = delete;
  ~Spring() = default;

  Position GetPosition(float start_offset, base::TimeDelta time) {
    // The general solution to a damped oscillator.
    const float a = undamped_natural_frequency_ * damping_ratio_;
    const float c =
        (initial_velocity_ + a * start_offset) / damped_natural_frequency_;
    const float ms = time.InMillisecondsF();
    const float offset =
        std::exp(-a * ms) *
        (c * std::sin(damped_natural_frequency_ * ms) +
         start_offset * std::cos(damped_natural_frequency_ * ms));

    spring_position_history_.push_back({.equilibrium_offset_viewport = offset,
                                        .timestamp = time,
                                        .at_rest = false});

    if (spring_position_history_.size() > kSpringHistorySize) {
      spring_position_history_.pop_front();
      float energy = 0;
      for (const auto& p : spring_position_history_) {
        // Energy is proportional to the square of the amplitude.
        energy += p.equilibrium_offset_viewport * p.equilibrium_offset_viewport;
      }
      // If the spring has used `kSpringAtRestThreshold * kSpringHistorySize`
      // amount energy in the last `kSpringHistorySize` locations, consider it
      // is at rest.
      spring_position_history_.back().at_rest |=
          energy < kSpringAtRestThreshold * kSpringHistorySize;
    }

    // Less than 1pixel from its equilibrium.
    spring_position_history_.back().at_rest |=
        offset <= 1.f / device_scale_factor_;

    return spring_position_history_.back();
  }

  float ComputeVelocity() {
    std::vector<float> timestamps;
    std::vector<float> positions;
    timestamps.reserve(spring_position_history_.size());
    positions.reserve(spring_position_history_.size());

    for (const auto& p : spring_position_history_) {
      timestamps.push_back(p.timestamp.InMillisecondsF());
      positions.push_back(p.equilibrium_offset_viewport);
    }

    float velocity = 0;
    SolveLeastSquare(timestamps, positions, &velocity);

    return velocity;
  }

  float initial_velocity() const { return initial_velocity_; }
  void set_initial_velocity(float velocity) { initial_velocity_ = velocity; }

 private:
  // Intrinsic properties of the spring.
  const int frequency_response_;
  const float damping_ratio_;
  const float device_scale_factor_;
  float undamped_natural_frequency_;
  float damped_natural_frequency_;
  const float mass_ = 1.f;

  // The initial velocity might not be zero: to enture the smooth animation
  // hand-off from spring A to spring B, we might set B's initial velocity to
  // A's terminal velocity.
  float initial_velocity_ = 0.f;

  // The last few positions of the spring. Used to interpolate the velocity. It
  // has a max size of `kSpringHistorySize`.
  std::deque<Position> spring_position_history_;
};

PhysicsModel::PhysicsModel(int screen_width_physical, float device_scale_factor)
    : viewport_width_(screen_width_physical / device_scale_factor),
      device_scale_factor_(device_scale_factor) {
  spring_cancel_ = std::make_unique<Spring>(
      /*frequency_response=*/200,
      /*damping_ratio=*/0.9,
      /*device_scaling_factor=*/device_scale_factor_);
  spring_commit_pending_ = std::make_unique<Spring>(
      /*frequency_response=*/kSpringResponse,
      /*damping_ratio=*/kSpringDampingRatio,
      /*device_scaling_factor=*/device_scale_factor_);
  spring_invoke_ = std::make_unique<Spring>(
      /*frequency_response=*/200,
      /*damping_ratio=*/0.95, /*device_scaling_factor=*/device_scale_factor_);
}

PhysicsModel::~PhysicsModel() = default;

PhysicsModel::Result PhysicsModel::OnAnimate(
    base::TimeTicks request_animation_frame) {
  // `commit_pending_acceleration_start_` needs to be recorded before we switch
  // to the next driver.
  RecordCommitPendingAccelerationStartIfNeeded(request_animation_frame);

  AdvanceToNextAnimationDriver(request_animation_frame);

  base::TimeDelta raf_since_start =
      CalculateRequestAnimationFrameSinceStart(request_animation_frame);

  // Ask the animation driver for the offset of the next frame.
  Spring::Position spring_position;
  switch (animation_driver_) {
    case Driver::kSpringCommitPending: {
      spring_position = spring_commit_pending_->GetPosition(
          viewport_width_ * kTargetCommitPending -
              animation_start_offset_viewport_,
          raf_since_start);
      // Prevent overshoot.
      foreground_offset_viewport_ = std::min(
          viewport_width_, viewport_width_ * kTargetCommitPending -
                               spring_position.equilibrium_offset_viewport);
      break;
    }
    case Driver::kSpringInvoke: {
      spring_position = spring_invoke_->GetPosition(
          viewport_width_ - animation_start_offset_viewport_, raf_since_start);
      // Prevent overshoot.
      foreground_offset_viewport_ = std::min(
          viewport_width_,
          viewport_width_ - spring_position.equilibrium_offset_viewport);
      break;
    }
    case Driver::kSpringCancel: {
      spring_position = spring_cancel_->GetPosition(
          animation_start_offset_viewport_, raf_since_start);
      // Prevent overshoot.
      foreground_offset_viewport_ =
          std::max(spring_position.equilibrium_offset_viewport, 0.f);
      break;
    }
    case Driver::kDragCurve: {
      NOTREACHED_NORETURN();
    }
  }

  foreground_has_reached_target_commit_pending_ |=
      foreground_offset_viewport_ >= kTargetCommitPending * viewport_width_;

  last_request_animation_frame_ = request_animation_frame;

  return Result{
      .foreground_offset_physical =
          foreground_offset_viewport_ * device_scale_factor_,
      .background_offset_physical =
          ForegroundToBackGroundOffset(foreground_offset_viewport_) *
          device_scale_factor_,
      // Done only if we have finished playing the terminal animations.
      .done = (spring_position.at_rest &&
               (animation_driver_ == Driver::kSpringInvoke ||
                animation_driver_ == Driver::kSpringCancel)),
  };
}

// Note: we don't call `StartAnimating()` with the drag curve because
// `timestamp` for the drag curve is not from the wallclock. The non-wallclock
// time shouldn't be stored as `animation_start_time_`.
PhysicsModel::Result PhysicsModel::OnGestureProgressed(
    float movement_physical,
    base::TimeTicks timestamp) {
  CHECK_EQ(animation_driver_, Driver::kDragCurve);
  const float movement_viewport = movement_physical / device_scale_factor_;

  foreground_offset_viewport_ =
      std::max(0.f, FingerDragCurve(movement_viewport));
  touch_points_history_.push_back(TouchEvent{
      .position_viewport = foreground_offset_viewport_,
      .timestamp = timestamp,
  });
  if (touch_points_history_.size() > kPhysicsModelHistorySize) {
    touch_points_history_.pop_front();
  }
  return Result{
      .foreground_offset_physical =
          foreground_offset_viewport_ * device_scale_factor_,
      .background_offset_physical =
          ForegroundToBackGroundOffset(foreground_offset_viewport_) *
          device_scale_factor_,
      .done = false,
  };
}

void PhysicsModel::OnGestureDone(bool commit) {
  // The user has lifted the finger. The previous animations must be driven by
  // the finger drag curve.
  CHECK_EQ(animation_driver_, Driver::kDragCurve);
  // We don't store the dummy timeticks for finger drag curve.
  CHECK(last_request_animation_frame_.is_null());
  // The navigation just started by the caller in the same atomic callstack. The
  // navigation hasn't committed or been cancelled yet.
  CHECK_EQ(navigation_state_, NavigationTerminalState::kNotSet);

  display_cancel_animation_ = !commit;

  // We will switch to `OnAnimate()`, where we drive the animation one of the
  // spring models.
}

void PhysicsModel::OnDidFinishNavigation(bool committed) {
  // Can only be called once.
  CHECK_EQ(navigation_state_, NavigationTerminalState::kNotSet);
  // Only allowed to call this API for commit-pending state. This is because the
  // navigation only starts after the user lifts the finger
  // (Driver::kDragCurve) and the physics model won't switch to any other driver
  // until this API is called.
  //
  // The navigation can also be fast enough for the commit-pending to not play
  // even a single frame (i.e., OnAnimate() not even called once by the OS,
  // after the user lifts the finger, so that PhysicsModel never gets to
  // advance from kDragCurve to kSpringCommitPending).
  CHECK(animation_driver_ == Driver::kSpringCommitPending ||
        animation_driver_ == Driver::kDragCurve);

  navigation_state_ = committed ? NavigationTerminalState::kCommitted
                                : NavigationTerminalState::kCancelled;
}

void PhysicsModel::StartAnimating(base::TimeTicks time) {
  animation_start_time_ = time;
  animation_start_offset_viewport_ = foreground_offset_viewport_;
}

float PhysicsModel::ForegroundToBackGroundOffset(float fg_offset) {
  if ((animation_driver_ == Driver::kSpringCommitPending ||
       animation_driver_ == Driver::kSpringInvoke) &&
      foreground_has_reached_target_commit_pending_) {
    // Do not bounce the background page when the foreground page has reached
    // the commit-pending point, once we have switched to the commit-pending
    // spring.
    return 0.f;
  }
  return std::min(0.f,
                  0.25f * (fg_offset - viewport_width_ * kTargetCommitPending));
}

float PhysicsModel::FingerDragCurve(float movement_viewport) {
  return foreground_offset_viewport_ + kTargetCommitPending * movement_viewport;
}

float PhysicsModel::CalculateVelocity() {
  float velocity = 0;

  std::vector<float> timestamps;
  std::vector<float> positions;
  timestamps.reserve(touch_points_history_.size());
  positions.reserve(touch_points_history_.size());
  for (const auto& p : touch_points_history_) {
    timestamps.push_back(
        (base::TimeTicks::Now() - p.timestamp).InMillisecondsF());
    positions.push_back(p.position_viewport);
  }
  SolveLeastSquare(timestamps, positions, &velocity);

  const float sign = velocity >= 0.f ? 1.f : -1.f;
  velocity = std::abs(velocity);

  // TODO(liuwilliam): Shall we let the UX team to fine-tune these?
  velocity = std::max(velocity, 1.0f);
  velocity = std::min(velocity, 2.5f);
  velocity = std::max(velocity, 0.3f);

  return velocity * sign;
}

void PhysicsModel::RecordCommitPendingAccelerationStartIfNeeded(
    base::TimeTicks request_animation_frame) {
  if (animation_driver_ == Driver::kSpringCommitPending &&
      navigation_state_ == NavigationTerminalState::kCommitted) {
    if (spring_commit_pending_->ComputeVelocity() > 0.f) {
      // If the navigation is committed and `spring_commit_pending_` is moving
      // at the opposite direction of the invoke animation, record the first
      // requested frame's timestamp. This timestamp will be used to speed up
      // the opposite-moving animation of the commit-pending spring. Since the
      // navigation is committed, we should display the invoke animation as soon
      // as possible.
      if (commit_pending_acceleration_start_.is_null()) {
        commit_pending_acceleration_start_ = request_animation_frame;
      }
    } else {
      // `spring_commit_pending_` moves in the same direction as the invoke
      // animation. Reset `commit_pending_acceleration_start_`.
      commit_pending_acceleration_start_ = base::TimeTicks();
    }
  }
}

void PhysicsModel::AdvanceToNextAnimationDriver(
    base::TimeTicks request_animation_frame) {
  switch (animation_driver_) {
    case Driver::kDragCurve: {
      // We can only reach here for once, and once only.
      CHECK(last_request_animation_frame_.is_null());
      StartAnimating(request_animation_frame);
      float finger_vel = CalculateVelocity();
      if (display_cancel_animation_ ||
          navigation_state_ == NavigationTerminalState::kCancelled) {
        animation_driver_ = Driver::kSpringCancel;
        // TODO(https://crbug.com/1504838): Least square can interpolate the
        // velocity in the wrong direction if the user swipes to the invoke
        // direction in the "cancel region" of the screen. For now, just use a
        // constant velocity.
        spring_cancel_->set_initial_velocity(1.f);
      } else if (navigation_state_ == NavigationTerminalState::kCommitted) {
        animation_driver_ = Driver::kSpringInvoke;
        spring_invoke_->set_initial_velocity(finger_vel);
      } else {
        animation_driver_ = Driver::kSpringCommitPending;
        spring_commit_pending_->set_initial_velocity(finger_vel);
      }
      break;
    }
    case Driver::kSpringCommitPending: {
      // It is rare but possible that we haven't played a single frame with
      // commit-pending spring, where `last_request_animation_frame_` is null.
      auto start_animating_raf = !last_request_animation_frame_.is_null()
                                     ? last_request_animation_frame_
                                     : request_animation_frame;
      if (commit_pending_acceleration_start_.is_null() &&
          navigation_state_ == NavigationTerminalState::kCommitted) {
        // Only switch from commit-pending spring to the invoke spring when:
        // - The commit-pending is moving in the same direction as the invoke
        //   animation, for which `commit_pending_acceleration_start_` is null.
        // - The navigation is committed.
        StartAnimating(start_animating_raf);
        animation_driver_ = Driver::kSpringInvoke;
        spring_invoke_->set_initial_velocity(
            spring_commit_pending_->ComputeVelocity());
      } else if (navigation_state_ == NavigationTerminalState::kCancelled) {
        StartAnimating(start_animating_raf);
        animation_driver_ = Driver::kSpringCancel;
        // TODO(https://crbug.com/1504838): Ditto.
        spring_cancel_->set_initial_velocity(1.f);
      } else {
        // Keep running the commit-pending animation if:
        // - The commit-pending animation is being accelerated, for which
        //   `last_request_animation_frame_` is non-null.
        // - The on-going navigation hasn't reached its final state
        //   (`OnDidFinishNavigation()` not yet called).
        const bool commit_pending_being_accelerated =
            (!last_request_animation_frame_.is_null() &&
             navigation_state_ == NavigationTerminalState::kCommitted);
        const bool nav_not_finish =
            navigation_state_ == NavigationTerminalState::kNotSet;
        CHECK(commit_pending_being_accelerated || nav_not_finish);
      }
      break;
    }
    // Shouldn't switch from the terminal states.
    case Driver::kSpringInvoke:
    case Driver::kSpringCancel:
      return;
  }

  if (IsInvalidVelocity(spring_invoke_->initial_velocity())) {
    spring_invoke_->set_initial_velocity(-2.0);
  }
  if (IsInvalidVelocity(spring_commit_pending_->initial_velocity())) {
    spring_commit_pending_->set_initial_velocity(0.f);
  }
  if (IsInvalidVelocity(spring_cancel_->initial_velocity())) {
    spring_cancel_->set_initial_velocity(1.f);
  }
}

base::TimeDelta PhysicsModel::CalculateRequestAnimationFrameSinceStart(
    base::TimeTicks request_animation_frame) {
  // Shouldn't be called for the drag curve animation.
  CHECK_NE(animation_driver_, Driver::kDragCurve);

  base::TimeDelta raf_since_start =
      request_animation_frame - animation_start_time_;

  // Accelerate the commit-pending animation if necessary.
  if (!commit_pending_acceleration_start_.is_null()) {
    CHECK_EQ(navigation_state_, NavigationTerminalState::kCommitted);
    CHECK_EQ(animation_driver_, Driver::kSpringCommitPending);
    // Add a delta to all the left-moving frames. This is to "speed up" the
    // spring animation, so it can start to move to the right sooner, to display
    // the invoke animation.
    //
    // Ex:
    // - request animation frame timeline: [37, 39, 41, 43, 45 ...]
    // - raf timeline with the delta:      [37, 41, 45, 49, 53 ...]
    //
    // So the net effect is the animation is sped up twice.
    raf_since_start +=
        (request_animation_frame - commit_pending_acceleration_start_);
  }

  return raf_since_start;
}

}  // namespace content
