// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_STRICT_YIELDING_DISPLAY_LOCK_BUDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_STRICT_YIELDING_DISPLAY_LOCK_BUDGET_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"

namespace blink {

// This budget yields between lifecycle phases even if that phase is quick. In
// other words, it will only do one new lifecycle phase at a time, and block the
// future ones. Any lifecycle phases that have already been allowed by this
// budget in the past are not blocked.
class CORE_EXPORT StrictYieldingDisplayLockBudget final
    : public DisplayLockBudget {
 public:
  StrictYieldingDisplayLockBudget(DisplayLockContext*);
  ~StrictYieldingDisplayLockBudget() override = default;

  bool ShouldPerformPhase(Phase, const LifecycleData&) override;
  void DidPerformPhase(Phase) override;
  void OnLifecycleChange(const LifecycleData&) override;
  // Returns true if any of the lifecycles that have been previously blocked by
  // this budget need updates. Note that this does not check lifecycle phases
  // that have already completed by this budget even if they are now dirty
  // again. This is done to prevent starvation (ie, it is possible for the
  // budget to always schedule more work if something in rAF keeps dirtying
  // layout, for example).
  bool NeedsLifecycleUpdates() const override;

 protected:
  base::Optional<Phase> last_completed_phase_;
  bool completed_new_phase_this_cycle_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_STRICT_YIELDING_DISPLAY_LOCK_BUDGET_H_
