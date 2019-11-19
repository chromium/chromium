// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/yielding_display_lock_budget.h"

#include "base/time/tick_clock.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"

#include <algorithm>

namespace blink {

YieldingDisplayLockBudget::YieldingDisplayLockBudget(
    DisplayLockContext* context)
    : DisplayLockBudget(context) {}

bool YieldingDisplayLockBudget::ShouldPerformPhase(
    Phase phase,
    const LifecycleData& lifecycle_data) {
  // Always perform at least one more phase.
  if (phase <= next_phase_from_start_of_lifecycle_)
    return true;

  // We should perform any phase earlier than the one we already completed.
  if (last_completed_phase_ && phase <= *last_completed_phase_)
    return true;

  // Otherwise, we can still do work while we're not past the deadline.
  return clock_->NowTicks() < deadline_;
}

void YieldingDisplayLockBudget::DidPerformPhase(Phase phase) {
  if (!last_completed_phase_ || phase > *last_completed_phase_)
    last_completed_phase_ = phase;

  // Mark the next phase as dirty so that we can reach it if we need to.
  MarkPhaseAsDirty(
      static_cast<Phase>(static_cast<unsigned>(*last_completed_phase_) + 1));
}

void YieldingDisplayLockBudget::OnLifecycleChange(
    const LifecycleData& lifecycle_data) {
  if (first_lifecycle_count_ == 0)
    first_lifecycle_count_ = lifecycle_data.count;
  deadline_ = lifecycle_data.start_time + GetCurrentBudget(lifecycle_data);

  // Figure out the next phase we would run. If we had completed a phase before,
  // then we should try to complete the next one, otherwise we'll start with the
  // first phase.
  next_phase_from_start_of_lifecycle_ =
      last_completed_phase_
          ? static_cast<Phase>(
                std::min(static_cast<unsigned>(*last_completed_phase_) + 1,
                         static_cast<unsigned>(Phase::kLast)))
          : Phase::kFirst;
  MarkPhaseAsDirty(next_phase_from_start_of_lifecycle_);
}

bool YieldingDisplayLockBudget::NeedsLifecycleUpdates() const {
  if (last_completed_phase_ && *last_completed_phase_ == Phase::kLast)
    return false;

  auto next_phase = last_completed_phase_
                        ? static_cast<Phase>(
                              static_cast<unsigned>(*last_completed_phase_) + 1)
                        : Phase::kFirst;
  // Check if any future phase needs updates.
  for (auto phase = static_cast<unsigned>(next_phase);
       phase <= static_cast<unsigned>(Phase::kLast); ++phase) {
    if (IsElementDirtyForPhase(static_cast<Phase>(phase)))
      return true;
  }
  return false;
}

base::TimeDelta YieldingDisplayLockBudget::GetCurrentBudget(
    const LifecycleData& lifecycle_data) const {
  int lifecycle_count = lifecycle_data.count - first_lifecycle_count_ + 1;
  if (base::TimeTicks::IsHighResolution()) {
    if (lifecycle_count < 3)
      return base::TimeDelta::FromMilliseconds(4);
    if (lifecycle_count < 10)
      return base::TimeDelta::FromMilliseconds(8);
    if (lifecycle_count < 60)
      return base::TimeDelta::FromMilliseconds(16);
  } else {
    // Without a high resolution clock, the resolution can be as bad as 15ms, so
    // increase the budgets accordingly to ensure we don't abort before doing
    // any phases.
    if (lifecycle_count < 60)
      return base::TimeDelta::FromMilliseconds(16);
  }
  return base::TimeDelta::FromMilliseconds(1e9);
}

}  // namespace blink
