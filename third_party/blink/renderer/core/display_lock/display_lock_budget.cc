// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"

#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

namespace blink {

DisplayLockBudget::DisplayLockBudget(DisplayLockContext* context)
    : clock_(base::DefaultTickClock::GetInstance()), context_(context) {}

bool DisplayLockBudget::MarkDirtyForPhaseIfNeeded(Phase phase) {
  switch (phase) {
    case Phase::kStyle:
      return context_->MarkForStyleRecalcIfNeeded();
    case Phase::kLayout:
      return context_->MarkForLayoutIfNeeded();
    case Phase::kPrePaint:
      return context_->MarkAncestorsForPrePaintIfNeeded();
  }
  NOTREACHED();
  return false;
}

bool DisplayLockBudget::IsElementDirtyForPhase(Phase phase) const {
  switch (phase) {
    case Phase::kStyle:
      return context_->IsElementDirtyForStyleRecalc();
    case Phase::kLayout:
      return context_->IsElementDirtyForLayout();
    case Phase::kPrePaint:
      return context_->IsElementDirtyForPrePaint();
  }
  NOTREACHED();
  return false;
}

void DisplayLockBudget::MarkPhaseAsDirty(Phase marking_phase) {
  // Mark the next phase we're scheduled to run.
  for (auto phase = static_cast<unsigned>(marking_phase);
       phase <= static_cast<unsigned>(Phase::kLast); ++phase) {
    if (MarkDirtyForPhaseIfNeeded(static_cast<Phase>(phase)))
      break;
  }
}

}  // namespace blink
