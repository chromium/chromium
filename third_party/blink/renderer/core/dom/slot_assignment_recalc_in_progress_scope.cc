// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/slot_assignment_recalc_in_progress_scope.h"

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

const char* InvariantName(SlotAssignmentInvariant invariant) {
  switch (invariant) {
    case SlotAssignmentInvariant::kNoScriptOrEventDispatch:
      return "script-or-event";
    case SlotAssignmentInvariant::kNoDOMMutation:
      return "dom-mutation";
    case SlotAssignmentInvariant::kNoFocusChange:
      return "focus-change";
    case SlotAssignmentInvariant::kNoAssignmentDirtying:
      return "dirtying";
    case SlotAssignmentInvariant::kNoReentrantRecalc:
      return "reentrant-recalc";
    case SlotAssignmentInvariant::kNoRecalcWhileForbidden:
      return "recalc-forbidden";
    case SlotAssignmentInvariant::kNoFlatTreeAccessWhileForbidden:
      return "flat-tree-forbidden";
  }
  NOTREACHED();
}

}  // namespace

unsigned SlotAssignmentRecalcInProgressScope::depth_ = 0;
unsigned
    SlotAssignmentRecalcInProgressScope::flat_tree_traversal_forbidden_depth_ =
        0;

SlotAssignmentRecalcInProgressScope::SlotAssignmentRecalcInProgressScope() {
  DCHECK(IsMainThread());
  ++depth_;
  CHECK_GT(depth_, 0u);
}

SlotAssignmentRecalcInProgressScope::~SlotAssignmentRecalcInProgressScope() {
  CHECK_GT(depth_, 0u);
  --depth_;
}

SlotAssignmentRecalcInProgressScope::ForbidFlatTreeTraversal::
    ForbidFlatTreeTraversal() {
  DCHECK(IsMainThread());
  ++flat_tree_traversal_forbidden_depth_;
  CHECK_GT(flat_tree_traversal_forbidden_depth_, 0u);
}

SlotAssignmentRecalcInProgressScope::ForbidFlatTreeTraversal::
    ~ForbidFlatTreeTraversal() {
  CHECK_GT(flat_tree_traversal_forbidden_depth_, 0u);
  --flat_tree_traversal_forbidden_depth_;
}

// static
void SlotAssignmentRecalcInProgressScope::ReportViolation(
    SlotAssignmentInvariant invariant) {
  if (!RuntimeEnabledFeatures::EnforceSlotAssignmentInvariantsEnabled()) {
    return;
  }
  SCOPED_CRASH_KEY_STRING32("SlotAssignment", "invariant",
                            InvariantName(invariant));
  NOTREACHED() << "Slot assignment recalc invariant violated: "
               << InvariantName(invariant);
}

// static
void SlotAssignmentRecalcInProgressScope::CheckNotActiveIfMainThread(
    SlotAssignmentInvariant invariant) {
  if (IsMainThread()) {
    CheckNotActive(invariant);
  }
}

}  // namespace blink
