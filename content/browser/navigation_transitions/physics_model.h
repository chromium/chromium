// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_TRANSITIONS_PHYSICS_MODEL_H_
#define CONTENT_BROWSER_NAVIGATION_TRANSITIONS_PHYSICS_MODEL_H_

#include <deque>
#include <memory>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// The spring models. Internal to `PhysicsModel`.
class Spring;

// The animation model that drives the animations for session history
// navigations. See `animation_driver_` for what this model is composed of.
class CONTENT_EXPORT PhysicsModel {
 public:
  // The live page of the current content will stop at 85% of the screen width
  // while wait for the navigation to the new page to commit.
  static constexpr float kTargetCommitPendingRatio = 0.85f;

  // Initially the screenshot is placed at (-0.25W, 0) with respect to the
  // viewport.
  static constexpr float kScreenshotInitialPositionRatio = -0.25f;

  // The calculated layer offsets by this physics model.
  struct Result {
    // The calculated offsets for the foreground and background layers. They are
    // physical pixel values.
    float foreground_offset_physical;
    float background_offset_physical;

    // Indicating if the animation has finished. Set to true when the invoke
    // animation or cancel animation has finished playing.
    bool done;
  };

  PhysicsModel(int screen_width_physical, float device_scale_factor);
  PhysicsModel(const PhysicsModel&) = delete;
  PhysicsModel& operator=(const PhysicsModel&) = delete;
  ~PhysicsModel();

  // Called when the user swipes the finger across the screen. Uses
  // `FingerDragCurve()` to calculate the layers' positions. `movement_physical`
  // is the delta pixels since the last user gesture, meaning it can be positive
  // or negative.
  Result OnGestureProgressed(float movement_physical,
                             base::TimeTicks timestamp);

  // Called when a frame is requested at `request_animation_frame`. Uses the
  // corresponding spring models to calculate the layers' positions. It is
  // called at each vsync, except when the UI thread is busy.
  Result OnAnimate(base::TimeTicks request_animation_frame);

  enum SwitchSpringReason {
    // Switch to `kSpringCancel` because the user lifts the finger and signals
    // to not start the navigation.
    kGestureCancelled = 0,
    // Switch to `kSpringCommitPending` because the user lifts the finger and
    // signals to start the navigation.
    kGestureInvoked,
    // Switch to `kSpringCancel` because the browser has dispatched the
    // BeforeUnload message to the renderer.
    kBeforeUnloadDispatched,
    // Switch to `kSpringCommitPending` because the renderer has acked to
    // proceed the navigation, in response to the BeforeUnload message.
    kBeforeUnloadAckProceed,
  };
  // Switch to a different spring model for various reasons.
  void SwitchSpringForReason(SwitchSpringReason reason);

  // Called when the navigation is finished (destruction of the navigation
  // request). The caller is responsible for reacting to the targeted navigation
  // request (when there are multiple navigation requests).
  void OnNavigationFinished(bool navigation_committed);

 private:
  // The "state" of the physics model. The animations can be driven by four
  // models:
  // - A drag curve when the user swipes across the screen and before the user
  //   lifts the finger.
  // - A spring model that bounces around the commit-pending point. It drives
  //   the commit-pending animation: to bounce the live page around the
  //   commit-pending point while waiting for the history navigation to commit.
  //   Equilibrium is the commit-pending position.
  // - A spring model that plays the invoke animation: to bring the page from
  //   the commit-pending point to completely out of the view port. Equilibrium
  //   is at the right edge for a back navigation.
  // - A spring model that plays the cancel animation: to bring the old live
  //   content back to the center of the viewport. Equilibrium is at the left
  //   edge for a back navigation.
  //
  // A big difference between a drag curve and a spring model is the drag curve
  // user-gesture driven while the spring models are vsync driven. The
  // implication is that for the drag curve the caller will need to provide a
  // timestamp associated with the finger movement, whereas for spring models we
  // get the timestamp from the wallclock.
  enum class Driver {
    kDragCurve = 0,
    kSpringCommitPending,
    kSpringInvoke,
    kSpringCancel,
  };

  // Record the starting point of the next animation driver. Called every time
  // `driver_` changes.
  void StartAnimating(base::TimeTicks time);

  // Calculates the background layer's viewport offset based on the foreground.
  float ForegroundToBackGroundOffset(float foreground_offset_viewport);

  // Calculate the foreground layer's viewport offset based on the finger's
  // movement.
  float FingerDragCurve(float movement_viewport);

  // Interpolates the velocity based off `touch_points_history_`. Used to set
  // the initial velocity of the spring model when the physics model switches
  // from drag cruve to any of the spring models.
  float CalculateVelocity();

  // Record `commit_pending_acceleration_start_`, if needed.
  void RecordCommitPendingAccelerationStartIfNeeded(
      base::TimeTicks request_animation_frame);

  // Advance the physics model to the next animation driver at
  // `request_animation_frame`. Updates `animation_driver_` and sets its initial
  // velocity. No-op for the terminal drivers (the invoke and cancel springs).
  void AdvanceToNextAnimationDriver(base::TimeTicks request_animation_frame);

  // Normalizes `request_animation_frame` with respect to the start of the
  // animation (i.e., when we first switched to the current animation driver).
  base::TimeDelta CalculateRequestAnimationFrameSinceStart(
      base::TimeTicks request_animation_frame);

  const float viewport_width_;

  // Used to convert the physical sizes into CSS/viewport sizes.
  const float device_scale_factor_;

  // Tracks the current state of the navigation.
  enum class NavigationState {
    kNotStarted = 0,
    // The navigation never starts. This is a terminal state for
    // `OnGestureCancelled()`.
    kNeverStarted,
    // The navigation has started, WITHOUT a BeforeUnload handler.
    kStarted,
    // The browser has sent the BeforeUnload message to the renderer.
    kBeforeUnloadDispatched,
    // The renderer has acked the BeforeUnload message and to start the
    // navigation.
    kBeforeUnloadAckedProceed,

    // No state when BeforeUnload ack'ed to not proceed.

    // The navigation has committed in the browser. This is one of the two
    // terminal states for `OnNavigationFinished()`.
    kCommitted,
    // The navigation is cancelled. This is another terminal state for
    // `OnNavigationFinished()`.
    kCancelled,
  };
  NavigationState navigation_state_ = NavigationState::kNotStarted;

  // The spring models correspond to
  // `Driver::{kSpringCommitPending|kSpringInvoke|kSpringCancel}`. See the
  // comments on `Driver` that describe the springs behavior. Always non-null.
  std::unique_ptr<Spring> spring_commit_pending_;
  std::unique_ptr<Spring> spring_invoke_;
  std::unique_ptr<Spring> spring_cancel_;

  // Wallclock.
  base::TimeTicks last_request_animation_frame_;

  // The physics model always starts with the drag curve.
  Driver animation_driver_ = Driver::kDragCurve;

  // Used to "speed up" the animation on `spring_commit_pending_` when the
  // invoke animation is ready to play. Set in
  // `RecordCommitPendingAccelerationStartIfNeeded()` and applied in
  // `CalculateRequestAnimationFrameSinceStart()`. Wallclock.
  base::TimeTicks commit_pending_acceleration_start_;

  // Wallclock.
  base::TimeTicks animation_start_time_;
  float animation_start_offset_viewport_ = 0.f;

  // Measured with respect to the left edge of the device.
  float foreground_offset_viewport_ = 0.f;
  bool foreground_has_reached_target_commit_pending_ = false;

  struct TouchEvent {
    float position_viewport;
    base::TimeTicks timestamp;
  };
  // Records the last few touch events. Used to interpolate the velocity. It has
  // a max size defined in the .cc file.
  std::deque<TouchEvent> touch_points_history_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_TRANSITIONS_PHYSICS_MODEL_H_
