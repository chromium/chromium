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
    : helper_(helper) {}

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

void ElasticOverscrollController::ObserveRealScrollBegin(OverscrollEntry& entry,
                                                         bool enter_momentum,
                                                         bool leave_momentum) {
  if (enter_momentum) {
    if (entry.state == kStateInactive) {
      entry.state = kStateMomentumScroll;
    }
  } else if (leave_momentum) {
    entry.scroll_velocity = gfx::Vector2dF();
    entry.last_scroll_event_timestamp = base::TimeTicks();
    entry.state = kStateActiveScroll;
    entry.pending_overscroll_delta = gfx::Vector2dF();
  }
}

void ElasticOverscrollController::ObserveScrollUpdate(
    OverscrollEntry& entry,
    const gfx::Vector2dF& event_delta,
    const gfx::Vector2dF& unused_scroll_delta,
    const base::TimeTicks& event_timestamp,
    const cc::OverscrollBehavior overscroll_behavior,
    bool has_momentum) {
  if (entry.state == kStateMomentumAnimated || entry.state == kStateInactive) {
    return;
  }

  if (!entry.received_overscroll_update && !unused_scroll_delta.IsZero()) {
    entry.overscroll_behavior = overscroll_behavior;
    entry.received_overscroll_update = true;
  }

  UpdateVelocity(entry, event_delta, event_timestamp);
  Overscroll(entry, unused_scroll_delta);
  if (has_momentum &&
      !helper_->StretchAmount(entry.target_scroller_id).IsZero()) {
    EnterStateMomentumAnimated(entry, event_timestamp);
  }
}

void ElasticOverscrollController::ObserveRealScrollEnd(
    OverscrollEntry& entry,
    const base::TimeTicks event_timestamp) {
  if (entry.state == kStateMomentumAnimated || entry.state == kStateInactive) {
    return;
  }

  if (helper_->StretchAmount(entry.target_scroller_id).IsZero()) {
    EnterStateInactive(entry);
  } else {
    EnterStateMomentumAnimated(entry, event_timestamp);
  }
}

void ElasticOverscrollController::ObserveGestureEventAndResult(
    cc::ElementId element_id,
    const WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  base::TimeTicks event_timestamp = gesture_event.TimeStamp();

  switch (gesture_event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin: {
      OverscrollEntry& entry = EnsureEntry(element_id);
      entry.received_overscroll_update = false;
      entry.overscroll_behavior = cc::OverscrollBehavior();
      if (gesture_event.data.scroll_begin.synthetic)
        return;

      bool enter_momentum = gesture_event.data.scroll_begin.inertial_phase ==
                            WebGestureEvent::InertialPhaseState::kMomentum;
      bool leave_momentum =
          gesture_event.data.scroll_begin.inertial_phase ==
              WebGestureEvent::InertialPhaseState::kNonMomentum &&
          gesture_event.data.scroll_begin.delta_hint_units ==
              ui::ScrollGranularity::kScrollByPrecisePixel;
      ObserveRealScrollBegin(entry, enter_momentum, leave_momentum);
      break;
    }
    case WebInputEvent::Type::kGestureScrollUpdate: {
      gfx::Vector2dF event_delta(-gesture_event.data.scroll_update.delta_x,
                                 -gesture_event.data.scroll_update.delta_y);
      bool has_momentum = gesture_event.data.scroll_update.inertial_phase ==
                          WebGestureEvent::InertialPhaseState::kMomentum;
      OverscrollEntry& entry = EnsureEntry(element_id);
      ObserveScrollUpdate(entry, event_delta, scroll_result.unused_scroll_delta,
                          event_timestamp, scroll_result.overscroll_behavior,
                          has_momentum);
      break;
    }
    case WebInputEvent::Type::kGestureScrollEnd: {
      if (gesture_event.data.scroll_end.synthetic)
        return;
      if (OverscrollEntry* entry = GetEntry(element_id)) {
        ObserveRealScrollEnd(*entry, event_timestamp);
      }
      break;
    }
    default:
      break;
  }
}

void ElasticOverscrollController::UpdateVelocity(
    OverscrollEntry& entry,
    const gfx::Vector2dF& event_delta,
    const base::TimeTicks& event_timestamp) {
  gfx::Vector2dF adjusted_overscroll_delta =
      helper_->ConstrainOverscrollDelta(entry.target_scroller_id, event_delta);

  float time_delta =
      (event_timestamp - entry.last_scroll_event_timestamp).InSecondsF();
  if (time_delta < kScrollVelocityZeroingTimeout && time_delta > 0) {
    entry.scroll_velocity =
        gfx::Vector2dF(adjusted_overscroll_delta.x() / time_delta,
                       adjusted_overscroll_delta.y() / time_delta);
  } else {
    entry.scroll_velocity = gfx::Vector2dF();
  }
  entry.last_scroll_event_timestamp = event_timestamp;
}

const ElasticOverscrollController::OverscrollEntry*
ElasticOverscrollController::GetEntry(cc::ElementId element_id) const {
  auto it = entries_.find(element_id);
  if (it != entries_.end()) {
    return it->second.get();
  }
  return nullptr;
}
ElasticOverscrollController::OverscrollEntry*
ElasticOverscrollController::GetEntry(cc::ElementId element_id) {
  return const_cast<OverscrollEntry*>(
      std::as_const(*this).GetEntry(element_id));
}

void ElasticOverscrollController::Overscroll(
    OverscrollEntry& entry,
    const gfx::Vector2dF& overscroll_delta) {

  // The effect can be dynamically disabled by setting styles to disallow user
  // scrolling. When disabled, disallow active or momentum overscrolling, but
  // allow any current overscroll to animate back.
  gfx::Vector2dF adjusted_overscroll_delta = helper_->ConstrainOverscrollDelta(
      entry.target_scroller_id, overscroll_delta);
  if (!helper_->IsUserOverscrollable(entry.target_scroller_id)) {
    adjusted_overscroll_delta = gfx::Vector2dF();
  }

  if (adjusted_overscroll_delta.IsZero())
    return;

  adjusted_overscroll_delta += entry.pending_overscroll_delta;
  entry.pending_overscroll_delta = gfx::Vector2dF();

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
    if (!CanScrollHorizontally(entry.target_scroller_id)) {
      adjusted_overscroll_delta.set_x(0);
    }
    if (!CanScrollVertically(entry.target_scroller_id)) {
      adjusted_overscroll_delta.set_y(0);
    }
  }

  // Don't allow overscrolling in a direction where scrolling is possible.
  if (!PinnedHorizontally(entry.target_scroller_id,
                          adjusted_overscroll_delta.x())) {
    adjusted_overscroll_delta.set_x(0);
  }
  if (!PinnedVertically(entry.target_scroller_id,
                        adjusted_overscroll_delta.y())) {
    adjusted_overscroll_delta.set_y(0);
  }

  // Don't allow overscrolling in a direction that has
  // OverscrollBehaviorTypeNone.
  if (entry.overscroll_behavior.x == cc::OverscrollBehavior::Type::kNone) {
    adjusted_overscroll_delta.set_x(0);
  }
  if (entry.overscroll_behavior.y == cc::OverscrollBehavior::Type::kNone) {
    adjusted_overscroll_delta.set_y(0);
  }

  // Require a minimum of 10 units of overscroll before starting the rubber-band
  // stretch effect, so that small stray motions don't trigger it. If that
  // minimum isn't met, save what remains in |pending_overscroll_delta| for
  // the next event.
  gfx::Vector2dF old_stretch_amount =
      helper_->StretchAmount(entry.target_scroller_id);
  gfx::Vector2dF stretch_scroll_force_delta;
  if (old_stretch_amount.x() != 0 ||
      fabsf(adjusted_overscroll_delta.x()) >=
          kRubberbandMinimumRequiredDeltaBeforeStretch) {
    stretch_scroll_force_delta.set_x(adjusted_overscroll_delta.x());
  } else {
    entry.pending_overscroll_delta.set_x(adjusted_overscroll_delta.x());
  }
  if (old_stretch_amount.y() != 0 ||
      fabsf(adjusted_overscroll_delta.y()) >=
          kRubberbandMinimumRequiredDeltaBeforeStretch) {
    stretch_scroll_force_delta.set_y(adjusted_overscroll_delta.y());
  } else {
    entry.pending_overscroll_delta.set_y(adjusted_overscroll_delta.y());
  }

  // Update the stretch amount according to the spring equations.
  if (stretch_scroll_force_delta.IsZero())
    return;
  entry.stretch_scroll_force += stretch_scroll_force_delta;
  gfx::Vector2dF new_stretch_amount =
      StretchAmountForAccumulatedOverscroll(entry, entry.stretch_scroll_force);
  helper_->SetStretchAmount(entry.target_scroller_id, new_stretch_amount);
}

void ElasticOverscrollController::EnterStateInactive(OverscrollEntry& entry) {
  DCHECK_NE(kStateInactive, entry.state);
  DCHECK(helper_->StretchAmount(entry.target_scroller_id).IsZero());
  // The entry is not erased from the map to avoid potential iterator
  // invalidation.
  entry.state = kStateInactive;

  entry.stretch_scroll_force = gfx::Vector2dF();
}

void ElasticOverscrollController::EnterStateMomentumAnimated(
    OverscrollEntry& entry,
    const base::TimeTicks& triggering_event_timestamp) {
  DCHECK_NE(kStateMomentumAnimated, entry.state);
  entry.state = kStateMomentumAnimated;

  // If the scroller isn't stretched, there's nothing to animate.
  if (helper_->StretchAmount(entry.target_scroller_id).IsZero()) {
    return;
  }

  entry.momentum_animation_start_time = triggering_event_timestamp;
  entry.momentum_animation_initial_stretch =
      helper_->StretchAmount(entry.target_scroller_id);
  entry.momentum_animation_initial_velocity = entry.scroll_velocity;

  // Similarly to the logic in Overscroll, prefer vertical scrolling to
  // horizontal scrolling.
  if (fabsf(entry.momentum_animation_initial_velocity.y()) >=
      fabsf(entry.momentum_animation_initial_velocity.x())) {
    entry.momentum_animation_initial_velocity.set_x(0);
  }

  if (!CanScrollHorizontally(entry.target_scroller_id)) {
    entry.momentum_animation_initial_velocity.set_x(0);
  }

  if (!CanScrollVertically(entry.target_scroller_id)) {
    entry.momentum_animation_initial_velocity.set_y(0);
  }

  DidEnterMomentumAnimatedState(entry);

  // TODO(crbug.com/394562): This can go away once input is batched to the front
  // of the frame? Then Animate() would always happen after this, so it would
  // have a chance to tick the animation there and would return if any
  // animations were active.
  helper_->RequestOneBeginFrame();
}

void ElasticOverscrollController::Animate(base::TimeTicks time) {
  bool is_animating = false;
  for (auto& [element_id, entry_ptr] : entries_) {
    OverscrollEntry& entry = *entry_ptr;
    if (entry.state != kStateMomentumAnimated) {
      continue;
    }

    // If the new stretch amount is near zero, set it directly to zero and enter
    // the inactive state.
    const gfx::Vector2dF new_stretch_amount = StretchAmountForTimeDelta(
        entry, std::max(time - entry.momentum_animation_start_time,
                        base::TimeDelta()));
    if (fabs(new_stretch_amount.x()) < 1 && fabs(new_stretch_amount.y()) < 1) {
      helper_->SetStretchAmount(element_id, gfx::Vector2dF());
      helper_->AnimationFinished(element_id);
      EnterStateInactive(entry);
      continue;
    }

    entry.stretch_scroll_force =
        AccumulatedOverscrollForStretchAmount(entry, new_stretch_amount);
    helper_->SetStretchAmount(element_id, new_stretch_amount);
    is_animating = true;
  }
  base::EraseIf(entries_, [](const auto& pair) {
    return pair.second->state == kStateInactive;
  });

  if (is_animating) {
    // TODO(danakj): Make this a return value back to the compositor to have it
    // schedule another frame and/or a draw. (Also, crbug.com/551138.)
    helper_->RequestOneBeginFrame();
  }
}

bool ElasticOverscrollController::PinnedHorizontally(cc::ElementId element_id,
                                                     float direction) const {
  gfx::PointF scroll_offset = helper_->ScrollOffset(element_id);
  gfx::PointF max_scroll_offset = helper_->MaxScrollOffset(element_id);
  if (direction < 0)
    return scroll_offset.x() <= 0;
  if (direction > 0)
    return scroll_offset.x() >= max_scroll_offset.x();
  return false;
}

bool ElasticOverscrollController::PinnedVertically(cc::ElementId element_id,
                                                   float direction) const {
  gfx::PointF scroll_offset = helper_->ScrollOffset(element_id);
  gfx::PointF max_scroll_offset = helper_->MaxScrollOffset(element_id);
  if (direction < 0)
    return scroll_offset.y() <= 0;
  if (direction > 0)
    return scroll_offset.y() >= max_scroll_offset.y();
  return false;
}

bool ElasticOverscrollController::CanScrollHorizontally(
    cc::ElementId element_id) const {
  return helper_->MaxScrollOffset(element_id).x() > 0;
}

bool ElasticOverscrollController::CanScrollVertically(
    cc::ElementId element_id) const {
  return helper_->MaxScrollOffset(element_id).y() > 0;
}

std::unique_ptr<ElasticOverscrollController::OverscrollEntry>
ElasticOverscrollController::CreateOverscrollEntry(
    cc::ElementId target_scroller_id) {
  return std::make_unique<OverscrollEntry>(target_scroller_id);
}

ElasticOverscrollController::OverscrollEntry&
ElasticOverscrollController::EnsureEntry(cc::ElementId element_id) {
  auto it = entries_.find(element_id);
  if (it == entries_.end()) {
    it = entries_.emplace(element_id, CreateOverscrollEntry(element_id)).first;
  }
  return *it->second;
}

void ElasticOverscrollController::ReconcileStretchAndScroll() {
  for (auto& [element_id, entry_ptr] : entries_) {
    OverscrollEntry& entry = *entry_ptr;
    gfx::Vector2dF stretch = helper_->StretchAmount(element_id);
    if (stretch.IsZero()) {
      continue;
    }

    gfx::PointF scroll_offset = helper_->ScrollOffset(element_id);
    gfx::PointF max_scroll_offset = helper_->MaxScrollOffset(element_id);

    // Compute stretch_adjustment which will be added to |stretch| and
    // subtracted from the |scroll_offset|.
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

    if (stretch_adjustment.IsZero()) {
      continue;
    }

    gfx::Vector2dF new_stretch_amount = stretch + stretch_adjustment;
    helper_->ScrollBy(element_id, -stretch_adjustment);
    helper_->SetStretchAmount(element_id, new_stretch_amount);

    // Update the internal state for the active scroll to avoid discontinuities.
    if (entry.state == kStateActiveScroll) {
      entry.stretch_scroll_force =
          AccumulatedOverscrollForStretchAmount(entry, new_stretch_amount);
    }
  }
}

gfx::Vector2dF ElasticOverscrollController::StretchAmount(
    cc::ElementId element_id) const {
  return helper_->StretchAmount(element_id);
}

bool ElasticOverscrollController::CanOverscroll(
    cc::ElementId element_id) const {
  return helper_->IsUserOverscrollable(element_id);
}

gfx::Size ElasticOverscrollController::ScrollBounds(
    cc::ElementId element_id) const {
  return helper_->ScrollBounds(element_id);
}

}  // namespace blink
