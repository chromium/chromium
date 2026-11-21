// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_RECALC_FORBIDDEN_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_RECALC_FORBIDDEN_SCOPE_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Marks a region where the tree is unstable (in the middle of a DOM mutation,
// during layout-tree detach, or during style, layout, or paint), so slot
// assignment recalc must not run (rule R5 in slot_assignment.h). This is
// enforced in release builds by SlotAssignment::RecalcAssignment().
class [[nodiscard]] SlotAssignmentRecalcForbiddenScope {
  STACK_ALLOCATED();

 public:
  explicit SlotAssignmentRecalcForbiddenScope(Document& document)
      : count_(document.SlotAssignmentRecalcForbiddenRecursionDepth()) {
    ++count_;
    CHECK_NE(count_, 0u);
  }

  SlotAssignmentRecalcForbiddenScope(
      const SlotAssignmentRecalcForbiddenScope&) = delete;
  SlotAssignmentRecalcForbiddenScope& operator=(
      const SlotAssignmentRecalcForbiddenScope&) = delete;
  ~SlotAssignmentRecalcForbiddenScope() {
    CHECK_NE(count_, 0u);
    --count_;
  }

 private:
  unsigned& count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_RECALC_FORBIDDEN_SCOPE_H_
