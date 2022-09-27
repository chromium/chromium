// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

SlotAssignmentEngine::SlotAssignmentEngine() {}

void SlotAssignmentEngine::AddShadowRootNeedingRecalc(ShadowRoot& shadow_root) {
  DCHECK(shadow_root.isConnected());
  DCHECK(shadow_root.NeedsSlotAssignmentRecalc());
  shadow_roots_needing_recalc_.insert(&shadow_root);
}

void SlotAssignmentEngine::RemoveShadowRootNeedingRecalc(
    ShadowRoot& shadow_root) {
  DCHECK(shadow_root.isConnected());
  DCHECK(!shadow_root.NeedsSlotAssignmentRecalc());
  DCHECK(shadow_roots_needing_recalc_.Contains(&shadow_root));
  shadow_roots_needing_recalc_.erase(&shadow_root);
}

void SlotAssignmentEngine::Connected(ShadowRoot& shadow_root) {
  if (shadow_root.NeedsSlotAssignmentRecalc())
    AddShadowRootNeedingRecalc(shadow_root);
}

void SlotAssignmentEngine::Disconnected(ShadowRoot& shadow_root) {
  if (shadow_root.NeedsSlotAssignmentRecalc()) {
    DCHECK(shadow_roots_needing_recalc_.Contains(&shadow_root));
    shadow_roots_needing_recalc_.erase(&shadow_root);
  } else {
    DCHECK(!shadow_roots_needing_recalc_.Contains(&shadow_root));
  }
}

void SlotAssignmentEngine::RecalcSlotAssignments() {
  if (shadow_roots_needing_recalc_.empty())
    return;
  TRACE_EVENT0("blink", "SlotAssignmentEngine::RecalcSlotAssignments");
  for (auto& shadow_root :
       HeapHashSet<WeakMember<ShadowRoot>>(shadow_roots_needing_recalc_)) {
    DCHECK(shadow_root->isConnected());
    DCHECK(shadow_root->NeedsSlotAssignmentRecalc());
    // SlotAssignment::RecalcAssignment() will remove its shadow root from
    // shadow_roots_needing_recalc_.
    shadow_root->GetSlotAssignment().RecalcAssignment();
  }
  DCHECK(shadow_roots_needing_recalc_.empty());
}

void SlotAssignmentEngine::Trace(Visitor* visitor) const {
  visitor->Trace(shadow_roots_needing_recalc_);
}

}  // namespace blink
