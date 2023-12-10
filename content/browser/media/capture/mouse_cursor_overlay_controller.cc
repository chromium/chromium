// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"

#include <cmath>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

// static
constexpr base::TimeDelta MouseCursorOverlayController::kIdleTimeout;

void MouseCursorOverlayController::Start(
    std::unique_ptr<Overlay> overlay,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  DCHECK(overlay);
  DCHECK(task_runner);

  Stop();
  tick_clock_ = base::DefaultTickClock::GetInstance();
  overlay_ = std::move(overlay);
  overlay_task_runner_ = std::move(task_runner);
  should_send_mouse_events_ =
      base::FeatureList::IsEnabled(blink::features::kCapturedMouseEvents);
}

void MouseCursorOverlayController::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  if (overlay_) {
    tick_clock_ = nullptr;
    overlay_task_runner_->DeleteSoon(FROM_HERE, overlay_.release());
    overlay_task_runner_ = nullptr;
    should_send_mouse_events_ = false;
  }
}

bool MouseCursorOverlayController::IsUserInteractingWithView() const {
  return mouse_move_behavior() == kRecentlyMovedOrClicked;
}

base::WeakPtr<MouseCursorOverlayController>
MouseCursorOverlayController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MouseCursorOverlayController::SendMouseEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  if (!overlay_task_runner_) {
    return;
  }
  CHECK(overlay_);

  if (!last_emitted_coordinates_.has_value() ||
      last_observed_coordinates_ != *last_emitted_coordinates_) {
    last_emitted_coordinates_ = last_observed_coordinates_;
    last_emitted_coordinates_time_ = tick_clock_->NowTicks();
    // base::Unretained(overlay_.get()) is safe because DeleteSoon() is used to
    // queue a task to free overlay_ and no more tasks can be queued after that.
    overlay_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Overlay::OnCapturedMouseEvent,
                                  base::Unretained(overlay_.get()),
                                  last_observed_coordinates_));
  }
}

void MouseCursorOverlayController::OnMouseCoordinatesUpdated(
    const gfx::Point& coordinates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  DCHECK(should_send_mouse_events_);
  last_observed_coordinates_ = coordinates;
  if (last_observed_coordinates_timer_.IsRunning()) {
    return;
  }

  base::TimeDelta wait_time;
  if (last_emitted_coordinates_) {
    // Ensure we wait at least kMinWaitInterval since the previous event.
    const base::TimeDelta time_since_last_event =
        tick_clock_->NowTicks() - last_emitted_coordinates_time_;
    wait_time = kMinWaitInterval - time_since_last_event;
  }
  if (wait_time.is_positive()) {
    // base::Unretained(this) is safe because we own
    // last_observed_coordinates_timer_ and its destructor calls
    // TimerBase::AbandonScheduledTask().
    last_observed_coordinates_timer_.Start(
        FROM_HERE, wait_time,
        base::BindRepeating(&MouseCursorOverlayController::SendMouseEvent,
                            base::Unretained(this)));
  } else {
    SendMouseEvent();
  }
}

void MouseCursorOverlayController::OnMouseMoved(const gfx::PointF& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  switch (mouse_move_behavior()) {
    case kNotMoving:
      set_mouse_move_behavior(kStartingToMove);
      mouse_move_start_location_ = location;
      mouse_activity_ended_timer_.Reset();
      break;
    case kStartingToMove:
      if (std::abs(location.x() - mouse_move_start_location_.x()) >
              kMinMovementPixels ||
          std::abs(location.y() - mouse_move_start_location_.y()) >
              kMinMovementPixels) {
        set_mouse_move_behavior(kRecentlyMovedOrClicked);
        mouse_activity_ended_timer_.Reset();
      }
      break;
    case kRecentlyMovedOrClicked:
      mouse_activity_ended_timer_.Reset();
      break;
  }

  if (mouse_move_behavior() == kRecentlyMovedOrClicked) {
    UpdateOverlay(location);
  }
}

void MouseCursorOverlayController::OnMouseClicked(const gfx::PointF& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  mouse_activity_ended_timer_.Reset();
  set_mouse_move_behavior(kRecentlyMovedOrClicked);

  UpdateOverlay(location);
}

void MouseCursorOverlayController::OnMouseHasGoneIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // Note that the following is not redundant since callers other than the timer
  // may have invoked this method.
  mouse_activity_ended_timer_.Stop();

  set_mouse_move_behavior(kNotMoving);

  UpdateOverlay(gfx::PointF());
}

void MouseCursorOverlayController::UpdateOverlay(const gfx::PointF& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  if (!overlay_task_runner_) {
    return;
  }
  CHECK(overlay_);

  // Breaking out of the following do-block indicates one or more prerequisites
  // are not met and the cursor should be(come) hidden.
  do {
    // If the mouse has not recently moved, hide the overlay.
    if (mouse_move_behavior() != kRecentlyMovedOrClicked) {
      break;
    }

    const gfx::NativeCursor cursor = GetCurrentCursorOrDefault();
    const gfx::RectF relative_bounds =
        ComputeRelativeBoundsForOverlay(cursor, location);

    // If the cursor (and, by implication, the cursor image) has not changed,
    // just move the overlay to its new position, if any.
    if (cursor == last_cursor_) {
      if (bounds_ != relative_bounds) {
        bounds_ = relative_bounds;
        // base::Unretained(overlay_.get()) is safe because DeleteSoon() is used
        // to queue a task to free overlay_ and no more tasks can be queued
        // after that.
        overlay_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&Overlay::SetBounds,
                           base::Unretained(overlay_.get()), bounds_));
      }
      return;
    }

    // The cursor image has changed. Edge-case: If the platform does not provide
    // a cursor image (e.g., this can occur at browser shutdown), just hide the
    // overlay.
    const SkBitmap cursor_image = GetCursorImage(cursor);
    if (cursor_image.drawsNothing()) {
      last_cursor_ = gfx::NativeCursor();
      break;
    }
    last_cursor_ = cursor;
    bounds_ = relative_bounds;
    // base::Unretained(overlay_.get()) is safe because DeleteSoon() is used to
    // queue a task to free overlay_ and no more tasks can be queued after that.
    overlay_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Overlay::SetImageAndBounds,
                                  base::Unretained(overlay_.get()),
                                  cursor_image, bounds_));
    return;
  } while (false);

  // If this point has been reached, then the overlay should be hidden.
  if (!bounds_.IsEmpty()) {
    bounds_ = gfx::RectF();
    // base::Unretained(overlay_.get()) is safe because DeleteSoon() is used to
    // queue a task to free overlay_ and no more tasks can be queued after that.
    overlay_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Overlay::SetBounds,
                                  base::Unretained(overlay_.get()), bounds_));
  }
}

void MouseCursorOverlayController::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  tick_clock_ = tick_clock;
}

}  // namespace content
