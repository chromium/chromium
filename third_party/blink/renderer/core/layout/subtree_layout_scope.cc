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

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

SubtreeLayoutScope::SubtreeLayoutScope(LayoutObject& root) : root_(root) {
  CHECK(root_.GetDocument().View()->IsInPerformLayout());
}

SubtreeLayoutScope::~SubtreeLayoutScope() {
  CHECK(!root_.SelfNeedsLayout() ||
        root_.LayoutBlockedByDisplayLock(DisplayLockLifecycleTarget::kSelf));
  CHECK(!root_.NeedsLayout() || root_.LayoutBlockedByDisplayLock(
                                    DisplayLockLifecycleTarget::kChildren));

#if DCHECK_IS_ON()
  for (auto* layout_object : layout_objects_to_layout_)
    layout_object->AssertLaidOut();
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
