// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_BUDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_BUDGET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace base {
class TickClock;
}

namespace blink {

class DisplayLockContext;
struct LifecycleData;

class CORE_EXPORT DisplayLockBudget {
 public:
  enum class Phase : unsigned {
    kStyle,
    kLayout,
    kPrePaint,
    kFirst = kStyle,
    kLast = kPrePaint
  };

  DisplayLockBudget(DisplayLockContext*);
  virtual ~DisplayLockBudget() = default;

  // Returns true if the given phase is allowed to proceed under the current
  // budget.
  virtual bool ShouldPerformPhase(Phase, const LifecycleData&) = 0;

  // Called just before any calls to ShouldPerformPhase for a new lifecycle.
  virtual void OnLifecycleChange(const LifecycleData&) = 0;

  // Notifies the budget that the given phase was completed.
  virtual void DidPerformPhase(Phase) = 0;

  // Returns true if according to this budget, we still need a lifecycle update.
  // For example, if a budget blocked a needed phase, then it this will return
  // true indicating that another frame is needed.
  virtual bool NeedsLifecycleUpdates() const = 0;

  // The caller is the owner of the |clock|. The |clock| must outlive the
  // DisplayLockBudget.
  void SetTickClockForTesting(const base::TickClock* clock) { clock_ = clock; }

 protected:
  // Returns true if there is likely to be work for the given phase.
  bool IsElementDirtyForPhase(Phase) const;

  void MarkPhaseAsDirty(Phase marking_phase);

  // Marks the element and ancestor chain dirty for the given phase if it's
  // needed. Returns true if the ancestors were marked dirty and false
  // otherwise.
  bool MarkDirtyForPhaseIfNeeded(Phase);

  const base::TickClock* clock_;

 private:
  // This is a backpointer to the context, which should always outlive this
  // budget, so it's untraced.
  UntracedMember<DisplayLockContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_BUDGET_H_
