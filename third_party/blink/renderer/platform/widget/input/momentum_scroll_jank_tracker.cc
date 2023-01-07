// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/momentum_scroll_jank_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/platform/widget/input/event_with_callback.h"

namespace blink {

constexpr base::TimeDelta MomentumScrollJankTracker::kRecentEventCutoff;

MomentumScrollJankTracker::~MomentumScrollJankTracker() {
  // Don't log if this scroll had no momentum input (after the first event).
  if (total_event_count_ == 0)
    return;

  // We want to target 0 janks, so round our percentages up so that a single
  // jank over a large number of frames doesn't get lost.
  // Don't worry about overflow as we'd need a gesture > 100hrs long to hit,
  // and the downside is an incorrect metric.
  uint32_t rounding_factor = total_event_count_ - 1;
  uint32_t jank_percentage =
      (jank_count_ * 100 + rounding_factor) / total_event_count_;

  base::UmaHistogramPercentageObsoleteDoNotUse(
      "Renderer4.MomentumScrollJankPercentage", jank_percentage);
}

void MomentumScrollJankTracker::OnDispatchedInputEvent(
    EventWithCallback* event_with_callback,
    const base::TimeTicks& now) {
  DCHECK_EQ(event_with_callback->event().GetType(),
            WebGestureEvent::Type::kGestureScrollUpdate);

  const auto& gesture_event =
      static_cast<const WebGestureEvent&>(event_with_callback->event());

  // If the scroll is not in the momentum phase, it's driven by user input,
  // which happens at a higher frequency. Ignore this.
  if (gesture_event.data.scroll_update.inertial_phase !=
      WebGestureEvent::InertialPhaseState::kMomentum) {
    return;
  }

  // Ignore the first momentum input event, as this may happen out-of band
  // with BeginFrame, and may be coalesced in a non-jank scenario.
  // TODO(ericrk): Add a metric to track jank in the transition from user
  // to momentum input.
  if (!seen_first_momentum_input_) {
    seen_first_momentum_input_ = true;
    return;
  }

  total_event_count_ += event_with_callback->coalesced_count();

  jank_count_ +=
      event_with_callback->coalesced_count() - kExpectedMomentumEventsPerFrame;
}

}  // namespace blink
