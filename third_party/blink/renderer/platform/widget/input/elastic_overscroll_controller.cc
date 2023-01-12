// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller.h"

#include <math.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "cc/input/input_handler.h"
#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_bezier.h"
#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_exponential.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

namespace blink {

namespace {
constexpr double kScrollVelocityZeroingTimeout = 0.10f;
constexpr double kRubberbandMinimumRequiredDeltaBeforeStretch = 10;

#if BUILDFLAG(IS_ANDROID)
// On android, overscroll should not occur if the scroller is not scrollable in
// the overscrolled direction.
constexpr bool kOverscrollNonScrollableDirection = false;
#else   // BUILDFLAG(IS_ANDROID)
// On other platforms, overscroll can occur even if the scroller is not
// scrollable.
constexpr bool kOverscrollNonScrollableDirection = true;
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

ElasticOverscrollController::ElasticOverscrollController(
    cc::ScrollElasticityHelper* helper)
    : helper_(helper),
      state_(kStateInactive),
      received_overscroll_update_(false) {}

std::unique_ptr<ElasticOverscrollController>
ElasticOverscrollController::Create(cc::ScrollElasticityHelper* helper) {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kElasticOverscroll)
             ? std::make_unique<ElasticOverscrollControllerBezier>(helper)
             : nullptr;
#else
  return std::make_unique<ElasticOverscrollControllerExponential>(helper);
#endif
}

void ElasticOverscrollController::ObserveRealScrollBegin(bool enter_momentum,
                                                         bool leave_momentum) {
  if (enter_momentum) {
    if (state_ == kStateInactive)
      state_ = kStateMomentumScroll;
  } else if (leave_momentum) {
    scroll_velocity_ = gfx::Vector2dF();
    last_scroll_event_timestamp_ = base::TimeTicks();
    state_ = kStateActiveScroll;
    pending_overscroll_delta_ = gfx::Vector2dF();
  }
}

void ElasticOverscrollController::ObserveScrollUpdate(
    const gfx::Vector2dF& event_delta,
    const gfx::Vector2dF& unused_scroll_delta,
    const base::TimeTicks& event_timestamp,
    const cc::OverscrollBehavior overscroll_behavior,
    bool has_momentum) {
  if (state_ == kStateMomentumAnimated || state_ == kStateInactive)
    return;

  if (!received_overscroll_update_ && !unused_scroll_delta.IsZero()) {
    overscroll_behavior_ = overscroll_behavior;
    received_overscroll_update_ = true;
  }

  UpdateVelocity(event_delta, event_timestamp);
  Overscroll(unused_scroll_delta);
  if (has_momentum && !helper_->StretchAmount().IsZero())
    EnterStateMomentumAnimated(event_timestamp);
}

void ElasticOverscrollController::ObserveRealScrollEnd(
    const base::TimeTicks event_timestamp) {
  if (state_ == kStateMomentumAnimated || state_ == kStateInactive)
    return;

  if (helper_->StretchAmount().IsZero()) {
    EnterStateInactive();
  } else {
    EnterStateMomentumAnimated(event_timestamp);
  }
}

void ElasticOverscrollController::ObserveGestureEventAndResult(
    const WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  base::TimeTicks event_timestamp = gesture_event.TimeStamp();

  switch (gesture_event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin: {
      received_overscroll_update_ = false;
      overscroll_behavior_ = cc::OverscrollBehavior();
      if (gesture_event.data.scroll_begin.synthetic)
        return;

      bool enter_momentum = gesture_event.data.scroll_begin.inertial_phase ==
                            WebGestureEvent::InertialPhaseState::kMomentum;
      bool leave_momentum =
          gesture_event.data.scroll_begin.inertial_phase ==
              WebGestureEvent::InertialPhaseState::kNonMomentum &&
          gesture_event.data.scroll_begin.delta_hint_units ==
              ui::ScrollGranularity::kScrollByPrecisePixel;
      ObserveRealScrollBegin(enter_momentum, leave_momentum);
      break;
    }
    case WebInputEvent::Type::kGestureScrollUpdate: {
      gfx::Vector2dF event_delta(-gesture_event.data.scroll_update.delta_x,
                                 -gesture_event.data.scroll_update.delta_y);
      bool has_momentum = gesture_event.data.scroll_update.inertial_phase ==
                          WebGestureEvent::InertialPhaseState::kMomentum;
      ObserveScrollUpdate(event_delta, scroll_result.unused_scroll_delta,
                          event_timestamp, scroll_result.overscroll_behavior,
                          has_momentum);
      break;
    }
    case WebInputEvent::Type::kGestureScrollEnd: {
      if (gesture_event.data.scroll_end.synthetic)
        return;
      ObserveRealScrollEnd(event_timestamp);
      break;
    }
    default:
      break;
  }
}

void ElasticOverscrollController::UpdateVelocity(
    const gfx::Vector2dF& event_delta,
    const base::TimeTicks& event_timestamp) {
  float time_delta =
      (event_timestamp - last_scroll_event_timestamp_).InSecondsF();
  if (time_delta < kScrollVelocityZeroingTimeout && time_delta > 0) {
    scroll_velocity_ = gfx::Vector2dF(event_delta.x() / time_delta,
                                      event_delta.y() / time_delta);
  } else {
    scroll_velocity_ = gfx::Vector2dF();
  }
  last_scroll_event_timestamp_ = event_timestamp;
}

void ElasticOverscrollController::Overscroll(
    const gfx::Vector2dF& overscroll_delta) {
  gfx::Vector2dF adjusted_overscroll_delta = overscroll_delta;

  // The effect can be dynamically disabled by setting styles to disallow user
  // scrolling. When disabled, disallow active or momentum overscrolling, but
  // allow any current overscroll to animate back.
  if (!helper_->IsUserScrollableHorizontal())
    adjusted_overscroll_delta.set_x(0);
  if (!helper_->IsUserScrollableVertical())
    adjusted_overscroll_delta.set_y(0);

  if (adjusted_overscroll_delta.IsZero())
    return;

  adjusted_overscroll_delta += pending_overscroll_delta_;
  pending_overscroll_delta_ = gfx::Vector2dF();

  // TODO (arakeri): Make this prefer the writing mode direction instead.
  // Only allow one direction to overscroll at a time, and slightly prefer
  // scrolling vertically by applying the equal case to delta_y.
  if (fabsf(overscroll_delta.y()) >= fabsf(overscroll_delta.x()))
    adjusted_overscroll_delta.set_x(0);
  else
    adjusted_overscroll_delta.set_y(0);

  if (!kOverscrollNonScrollableDirection) {
    // Check whether each direction is scrollable and 0 out the overscroll if it
    // is not.
    if (!CanScrollHorizontally())
      adjusted_overscroll_delta.set_x(0);
    if (!CanScrollVertically())
      adjusted_overscroll_delta.set_y(0);
  }

  // Don't allow overscrolling in a direction where scrolling is possible.
  if (!PinnedHorizontally(adjusted_overscroll_delta.x()))
    adjusted_overscroll_delta.set_x(0);
  if (!PinnedVertically(adjusted_overscroll_delta.y()))
    adjusted_overscroll_delta.set_y(0);

  // Don't allow overscrolling in a direction that has
  // OverscrollBehaviorTypeNone.
  if (overscroll_behavior_.x == cc::OverscrollBehavior::Type::kNone)
    adjusted_overscroll_delta.set_x(0);
  if (overscroll_behavior_.y == cc::OverscrollBehavior::Type::kNone)
    adjusted_overscroll_delta.set_y(0);

  // Require a minimum of 10 units of overscroll before starting the rubber-band
  // stretch effect, so that small stray motions don't trigger it. If that
  // minimum isn't met, save what remains in |pending_overscroll_delta_| for
  // the next event.
  gfx::Vector2dF old_stretch_amount = helper_->StretchAmount();
  gfx::Vector2dF stretch_scroll_force_delta;
  if (old_stretch_amount.x() != 0 ||
      fabsf(adjusted_overscroll_delta.x()) >=
          kRubberbandMinimumRequiredDeltaBeforeStretch) {
    stretch_scroll_force_delta.set_x(adjusted_overscroll_delta.x());
  } else {
    pending_overscroll_delta_.set_x(adjusted_overscroll_delta.x());
  }
  if (old_stretch_amount.y() != 0 ||
      fabsf(adjusted_overscroll_delta.y()) >=
          kRubberbandMinimumRequiredDeltaBeforeStretch) {
    stretch_scroll_force_delta.set_y(adjusted_overscroll_delta.y());
  } else {
    pending_overscroll_delta_.set_y(adjusted_overscroll_delta.y());
  }

  // Update the stretch amount according to the spring equations.
  if (stretch_scroll_force_delta.IsZero())
    return;
  stretch_scroll_force_ += stretch_scroll_force_delta;
  gfx::Vector2dF new_stretch_amount =
      StretchAmountForAccumulatedOverscroll(stretch_scroll_force_);
  helper_->SetStretchAmount(new_stretch_amount);
}

void ElasticOverscrollController::EnterStateInactive() {
  DCHECK_NE(kStateInactive, state_);
  DCHECK(helper_->StretchAmount().IsZero());
  state_ = kStateInactive;
  stretch_scroll_force_ = gfx::Vector2dF();
}

void ElasticOverscrollController::EnterStateMomentumAnimated(
    const base::TimeTicks& triggering_event_timestamp) {
  DCHECK_NE(kStateMomentumAnimated, state_);
  state_ = kStateMomentumAnimated;

  // If the scroller isn't stretched, there's nothing to animate.
  if (helper_->StretchAmount().IsZero())
    return;

  momentum_animation_start_time_ = triggering_event_timestamp;
  momentum_animation_initial_stretch_ = helper_->StretchAmount();
  momentum_animation_initial_velocity_ = scroll_velocity_;

  // Similarly to the logic in Overscroll, prefer vertical scrolling to
  // horizontal scrolling.
  if (fabsf(momentum_animation_initial_velocity_.y()) >=
      fabsf(momentum_animation_initial_velocity_.x()))
    momentum_animation_initial_velocity_.set_x(0);

  if (!CanScrollHorizontally())
    momentum_animation_initial_velocity_.set_x(0);

  if (!CanScrollVertically())
    momentum_animation_initial_velocity_.set_y(0);

  DidEnterMomentumAnimatedState();

  // TODO(crbug.com/394562): This can go away once input is batched to the front
  // of the frame? Then Animate() would always happen after this, so it would
  // have a chance to tick the animation there and would return if any
  // animations were active.
  helper_->RequestOneBeginFrame();
}

void ElasticOverscrollController::Animate(base::TimeTicks time) {
  if (state_ != kStateMomentumAnimated)
    return;

  // If the new stretch amount is near zero, set it directly to zero and enter
  // the inactive state.
  const gfx::Vector2dF new_stretch_amount = StretchAmountForTimeDelta(
      std::max(time - momentum_animation_start_time_, base::TimeDelta()));
  if (fabs(new_stretch_amount.x()) < 1 && fabs(new_stretch_amount.y()) < 1) {
    helper_->SetStretchAmount(gfx::Vector2dF());
    EnterStateInactive();
    return;
  }

  stretch_scroll_force_ =
      AccumulatedOverscrollForStretchAmount(new_stretch_amount);
  helper_->SetStretchAmount(new_stretch_amount);
  // TODO(danakj): Make this a return value back to the compositor to have it
  // schedule another frame and/or a draw. (Also, crbug.com/551138.)
  helper_->RequestOneBeginFrame();
}

bool ElasticOverscrollController::PinnedHorizontally(float direction) const {
  gfx::PointF scroll_offset = helper_->ScrollOffset();
  gfx::PointF max_scroll_offset = helper_->MaxScrollOffset();
  if (direction < 0)
    return scroll_offset.x() <= 0;
  if (direction > 0)
    return scroll_offset.x() >= max_scroll_offset.x();
  return false;
}

bool ElasticOverscrollController::PinnedVertically(float direction) const {
  gfx::PointF scroll_offset = helper_->ScrollOffset();
  gfx::PointF max_scroll_offset = helper_->MaxScrollOffset();
  if (direction < 0)
    return scroll_offset.y() <= 0;
  if (direction > 0)
    return scroll_offset.y() >= max_scroll_offset.y();
  return false;
}

bool ElasticOverscrollController::CanScrollHorizontally() const {
  return helper_->MaxScrollOffset().x() > 0;
}

bool ElasticOverscrollController::CanScrollVertically() const {
  return helper_->MaxScrollOffset().y() > 0;
}

void ElasticOverscrollController::ReconcileStretchAndScroll() {
  gfx::Vector2dF stretch = helper_->StretchAmount();
  if (stretch.IsZero())
    return;

  gfx::PointF scroll_offset = helper_->ScrollOffset();
  gfx::PointF max_scroll_offset = helper_->MaxScrollOffset();

  // Compute stretch_adjustment which will be added to |stretch| and subtracted
  // from the |scroll_offset|.
  gfx::Vector2dF stretch_adjustment;
  if (stretch.x() < 0 && scroll_offset.x() > 0) {
    stretch_adjustment.set_x(
        std::min(-stretch.x(), static_cast<float>(scroll_offset.x())));
  }
  if (stretch.x() > 0 && scroll_offset.x() < max_scroll_offset.x()) {
    stretch_adjustment.set_x(std::max(
        -stretch.x(),
        static_cast<float>(scroll_offset.x() - max_scroll_offset.x())));
  }
  if (stretch.y() < 0 && scroll_offset.y() > 0) {
    stretch_adjustment.set_y(
        std::min(-stretch.y(), static_cast<float>(scroll_offset.y())));
  }
  if (stretch.y() > 0 && scroll_offset.y() < max_scroll_offset.y()) {
    stretch_adjustment.set_y(std::max(
        -stretch.y(),
        static_cast<float>(scroll_offset.y() - max_scroll_offset.y())));
  }

  if (stretch_adjustment.IsZero())
    return;

  gfx::Vector2dF new_stretch_amount = stretch + stretch_adjustment;
  helper_->ScrollBy(-stretch_adjustment);
  helper_->SetStretchAmount(new_stretch_amount);

  // Update the internal state for the active scroll to avoid discontinuities.
  if (state_ == kStateActiveScroll) {
    stretch_scroll_force_ =
        AccumulatedOverscrollForStretchAmount(new_stretch_amount);
  }
}

}  // namespace blink
