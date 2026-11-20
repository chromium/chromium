// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_recalc_in_progress_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

SlotAssignmentEngine::SlotAssignmentEngine() {}

void SlotAssignmentEngine::AddShadowRootNeedingRecalc(ShadowRoot& shadow_root) {
  DCHECK(shadow_root.NeedsSlotAssignmentRecalc());
  if (shadow_root.isConnected()) {
    shadow_roots_needing_recalc_.insert(&shadow_root);
  } else {
    disconnected_shadow_roots_needing_recalc_.insert(&shadow_root);
  }
}

void SlotAssignmentEngine::RemoveShadowRootNeedingRecalc(
    ShadowRoot& shadow_root) {
  DCHECK(!shadow_root.NeedsSlotAssignmentRecalc());
  if (shadow_root.isConnected()) {
    DCHECK(shadow_roots_needing_recalc_.Contains(&shadow_root));
  } else {
    DCHECK(disconnected_shadow_roots_needing_recalc_.Contains(&shadow_root));
  }
  shadow_roots_needing_recalc_.erase(&shadow_root);
  disconnected_shadow_roots_needing_recalc_.erase(&shadow_root);
}

void SlotAssignmentEngine::Connected(ShadowRoot& shadow_root) {
  // Note that ShadowRoot::InsertedInto() calls this after the shadow root's
  // connected flag is set.
  DCHECK(!shadow_roots_needing_recalc_.Contains(&shadow_root));
  if (shadow_root.NeedsSlotAssignmentRecalc()) {
    DCHECK(disconnected_shadow_roots_needing_recalc_.Contains(&shadow_root));
    disconnected_shadow_roots_needing_recalc_.erase(&shadow_root);
    shadow_roots_needing_recalc_.insert(&shadow_root);
  }
  // A clean shadow root can still have a stale entry in
  // disconnected_shadow_roots_needing_recalc_ if it was adopted into another
  // document while dirty and then back into this one, so that is not checked.
}

void SlotAssignmentEngine::Disconnected(ShadowRoot& shadow_root) {
  // Note that ShadowRoot::RemovedFrom() calls this before the shadow root's
  // connected flag is cleared.
  if (shadow_root.NeedsSlotAssignmentRecalc()) {
    DCHECK(shadow_roots_needing_recalc_.Contains(&shadow_root));
    shadow_roots_needing_recalc_.erase(&shadow_root);
    disconnected_shadow_roots_needing_recalc_.insert(&shadow_root);
  } else {
    DCHECK(!shadow_roots_needing_recalc_.Contains(&shadow_root));
  }
}

HeapVector<Member<ShadowRoot>> SlotAssignmentEngine::CollectShadowRootsToRecalc(
    ShadowRoot& shadow_root) {
  DCHECK(shadow_root.NeedsSlotAssignmentRecalc());
  HeapVector<Member<ShadowRoot>> roots;
  roots.push_back(&shadow_root);
  if (shadow_root.isConnected()) {
    DCHECK(shadow_roots_needing_recalc_.Contains(&shadow_root));
    for (auto& root : shadow_roots_needing_recalc_) {
      if (root != &shadow_root) {
        roots.push_back(root);
      }
    }
    return roots;
  }
  // Computing an assignment only reads the host's children and the shadow
  // root's slots, so roots in other trees can be left dirty. Entries can be
  // stale (e.g. after adoption into another document), so re-check them.
  const Node& tree_root = shadow_root.ShadowIncludingRoot();
  for (auto& root : disconnected_shadow_roots_needing_recalc_) {
    if (root == &shadow_root || root->isConnected() ||
        !root->NeedsSlotAssignmentRecalc() ||
        &root->ShadowIncludingRoot() != &tree_root) {
      continue;
    }
    roots.push_back(root);
  }
  return roots;
}

void SlotAssignmentEngine::RecalcSlotAssignments() {
  if (shadow_roots_needing_recalc_.empty())
    return;
  TRACE_EVENT0("blink", "SlotAssignmentEngine::RecalcSlotAssignments");
  HeapVector<Member<ShadowRoot>> roots;
  for (auto& shadow_root : shadow_roots_needing_recalc_) {
    DCHECK(shadow_root->isConnected());
    DCHECK(shadow_root->NeedsSlotAssignmentRecalc());
    roots.push_back(shadow_root);
  }
  // SlotAssignment::RecalcShadowRoots() removes each shadow root from
  // shadow_roots_needing_recalc_. If this runs during a recalc (which rule R4
  // forbids), RecalcShadowRoots() bails out and leaves them in the set.
  SlotAssignment::RecalcShadowRoots(roots);
  DCHECK(shadow_roots_needing_recalc_.empty() ||
         SlotAssignmentRecalcInProgressScope::IsActive());
}

void SlotAssignmentEngine::Trace(Visitor* visitor) const {
  visitor->Trace(shadow_roots_needing_recalc_);
  visitor->Trace(disconnected_shadow_roots_needing_recalc_);
}

}  // namespace blink
