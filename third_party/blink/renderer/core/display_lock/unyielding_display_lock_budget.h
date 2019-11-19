// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_UNYIELDING_DISPLAY_LOCK_BUDGET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_UNYIELDING_DISPLAY_LOCK_BUDGET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"

namespace blink {

// This budget never yields. That is, this is essentially infinite budget that
// will finish all of the lifecycle phases for the locked subtree if given the
// chance.
class CORE_EXPORT UnyieldingDisplayLockBudget final : public DisplayLockBudget {
 public:
  UnyieldingDisplayLockBudget(DisplayLockContext*);
  ~UnyieldingDisplayLockBudget() override = default;

  bool ShouldPerformPhase(Phase, const LifecycleData&) override;
  void DidPerformPhase(Phase) override;
  void OnLifecycleChange(const LifecycleData&) override;
  bool NeedsLifecycleUpdates() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_UNYIELDING_DISPLAY_LOCK_BUDGET_H_
