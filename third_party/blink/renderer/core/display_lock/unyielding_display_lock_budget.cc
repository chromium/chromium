// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/unyielding_display_lock_budget.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink {

UnyieldingDisplayLockBudget::UnyieldingDisplayLockBudget(
    DisplayLockContext* context)
    : DisplayLockBudget(context) {}

bool UnyieldingDisplayLockBudget::ShouldPerformPhase(
    Phase,
    const LifecycleData& lifecycle_data) {
  return true;
}

void UnyieldingDisplayLockBudget::DidPerformPhase(Phase) {}

void UnyieldingDisplayLockBudget::OnLifecycleChange(
    const LifecycleData& lifecycle_data) {
  // Mark all the phases dirty since we have no intention of yielding.
  for (auto phase = static_cast<unsigned>(Phase::kFirst);
       phase <= static_cast<unsigned>(Phase::kLast); ++phase) {
    MarkDirtyForPhaseIfNeeded(static_cast<Phase>(phase));
  }
}

bool UnyieldingDisplayLockBudget::NeedsLifecycleUpdates() const {
  for (auto phase = static_cast<unsigned>(Phase::kFirst);
       phase <= static_cast<unsigned>(Phase::kLast); ++phase) {
    if (IsElementDirtyForPhase(static_cast<Phase>(phase)))
      return true;
  }
  return false;
}

}  // namespace blink
