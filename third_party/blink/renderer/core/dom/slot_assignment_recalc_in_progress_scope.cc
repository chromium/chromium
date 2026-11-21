// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/slot_assignment_recalc_in_progress_scope.h"

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

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
    SlotAssignmentInvariant) {
  // TODO(crbug.com/582346913): Report violations once the known violators are
  // fixed.
}

// static
void SlotAssignmentRecalcInProgressScope::CheckNotActiveIfMainThread(
    SlotAssignmentInvariant invariant) {
  if (IsMainThread()) {
    CheckNotActive(invariant);
  }
}

}  // namespace blink
