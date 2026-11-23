// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_RECALC_IN_PROGRESS_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_RECALC_IN_PROGRESS_SCOPE_H_

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// The rules that slot assignment recalc must follow. See the SlotAssignment
// class comment in slot_assignment.h for the full description of each rule
// (R1-R7 there).
enum class SlotAssignmentInvariant {
  // R1. No script and no event dispatch during recalc.
  kNoScriptOrEventDispatch,
  // R2. No DOM mutation during recalc.
  kNoDOMMutation,
  // R3. No focus change during recalc.
  kNoFocusChange,
  // R4. Recalc does not dirty slot assignment.
  kNoAssignmentDirtying,
  // R4. Recalc does not re-enter recalc.
  kNoReentrantRecalc,
  // R5. Recalc is never triggered while the tree is unstable.
  kNoRecalcWhileForbidden,
  // R7. The lazy flat tree entry points are not used while flat tree
  // traversal is forbidden (while slot assignment data is being recomputed).
  kNoFlatTreeAccessWhileForbidden,
};

// Held for the whole of a slot assignment recalc, including its side effects
// (see SlotAssignment). The counter is global rather than per-document,
// because recalc side effects can reach other documents (e.g. canvas subtree
// propagation into a child frame). The scope also forbids script as a
// backstop.
//
// SlotAssignmentInvariant checks at script, event, mutation, and focus entry
// points call CheckNotActive().
class CORE_EXPORT [[nodiscard]] SlotAssignmentRecalcInProgressScope final {
  STACK_ALLOCATED();

 public:
  SlotAssignmentRecalcInProgressScope();
  ~SlotAssignmentRecalcInProgressScope();
  SlotAssignmentRecalcInProgressScope(
      const SlotAssignmentRecalcInProgressScope&) = delete;
  SlotAssignmentRecalcInProgressScope& operator=(
      const SlotAssignmentRecalcInProgressScope&) = delete;

  // True while slot assignment recalc is running, in any document.
  static bool IsActive() { return depth_ > 0; }

  // True while slot assignment data (assigned nodes, flat tree children, and
  // FlatTreeNodeData) is being recomputed. The flat tree must not be read
  // then, because that data is only partly updated.
  static bool IsFlatTreeTraversalForbidden() {
    return flat_tree_traversal_forbidden_depth_ > 0;
  }

  // Held while slot assignment data is being recomputed. See
  // IsFlatTreeTraversalForbidden().
  class CORE_EXPORT [[nodiscard]] ForbidFlatTreeTraversal final {
    STACK_ALLOCATED();

   public:
    ForbidFlatTreeTraversal();
    ~ForbidFlatTreeTraversal();
    ForbidFlatTreeTraversal(const ForbidFlatTreeTraversal&) = delete;
    ForbidFlatTreeTraversal& operator=(const ForbidFlatTreeTraversal&) = delete;
  };

  // Reports a violation of `invariant` if `violated` is true, and returns
  // `violated`, so that callers that need a fallback can branch on it. Callers
  // that only report can ignore the result. If the
  // EnforceSlotAssignmentInvariants feature is enabled, a violation is fatal
  // in all builds (with a crash key naming the rule). The feature has a kill
  // switch: while it is disabled, violations are not reported, so every
  // caller must stay safe after a violation.
  ALWAYS_INLINE static bool ReportViolationIf(
      bool violated,
      SlotAssignmentInvariant invariant) {
    if (violated) [[unlikely]] {
      ReportViolation(invariant);
    }
    return violated;
  }

  // Reports a violation of `invariant` if slot assignment recalc is running.
  // Must be called on the main thread.
  ALWAYS_INLINE static void CheckNotActive(SlotAssignmentInvariant invariant) {
    ReportViolationIf(IsActive(), invariant);
  }

  // Same as CheckNotActive(), but does nothing off the main thread, for entry
  // points that can also run on worker threads. Slot assignment recalc only
  // runs on the main thread.
  static void CheckNotActiveIfMainThread(SlotAssignmentInvariant invariant);

 private:
  NOINLINE static void ReportViolation(SlotAssignmentInvariant invariant);

  static unsigned depth_;
  static unsigned flat_tree_traversal_forbidden_depth_;

  ScriptForbiddenScope forbid_script_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_RECALC_IN_PROGRESS_SCOPE_H_
