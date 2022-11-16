/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/subtree_layout_scope.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

SubtreeLayoutScope::SubtreeLayoutScope(LayoutObject& root) : root_(root) {
  CHECK(root_.GetDocument().View()->IsInPerformLayout());
}

SubtreeLayoutScope::~SubtreeLayoutScope() {
  CHECK(!root_.SelfNeedsLayout());
  CHECK(!root_.NeedsLayout() || root_.ChildLayoutBlockedByDisplayLock());

#if DCHECK_IS_ON()
  for (const auto& layout_object : layout_objects_to_layout_) {
    // When CSS Container Queries are enabled, style recalc and layout tree
    // rebuild for a container during layout may detach LayoutObjects which
    // have been marked for layout. Skip such LayoutObject to avoid that
    // NOT_DESTROYED() triggers a DCHECK failure in AssertLaidOut() or
    // AssertFragmentTree().
    if (layout_object->is_destroyed_) {
      continue;
    }
    // There are situations where the object to layout was never laid out, such
    // as if there was a display-locked descendant of the root and ancestor of
    // the object which prevented layout. This can happen in quirks mode, where
    // an ancestor can mark a descendant as dirty through its
    // PercentHeightDescendants() list, which will not get cleared because
    // traversal is blocked by a display lock. This finds such cases and allows
    // these objects to be dirty.
    if (!DisplayLockUtilities::LockedAncestorPreventingLayout(*layout_object))
      layout_object->AssertLaidOut();
    layout_object->AssertFragmentTree();
  }
#endif
}

void SubtreeLayoutScope::SetNeedsLayout(
    LayoutObject* descendant,
    LayoutInvalidationReasonForTracing reason) {
  DCHECK(descendant->IsDescendantOf(&root_));
  descendant->SetNeedsLayout(reason, kMarkContainerChain, this);
}

void SubtreeLayoutScope::SetChildNeedsLayout(LayoutObject* descendant) {
  DCHECK(descendant->IsDescendantOf(&root_));
  descendant->SetChildNeedsLayout(kMarkContainerChain, this);
}

void SubtreeLayoutScope::RecordObjectMarkedForLayout(
    LayoutObject* layout_object) {
#if DCHECK_IS_ON()
  layout_objects_to_layout_.insert(layout_object);
#endif
}

}  // namespace blink
