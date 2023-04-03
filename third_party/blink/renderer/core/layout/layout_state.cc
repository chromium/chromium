/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_state.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

LayoutState::LayoutState(LayoutView& view)
    : containing_block_logical_width_changed_(false),
      next_(nullptr),
      layout_object_(&view) {
  DCHECK(!view.GetLayoutState());
  view.PushLayoutState(*this);
}

LayoutState::LayoutState(LayoutBox& layout_object,
                         bool containing_block_logical_width_changed)
    : containing_block_logical_width_changed_(
          containing_block_logical_width_changed),
      next_(layout_object.View()->GetLayoutState()),
      layout_object_(&layout_object) {
  layout_object.View()->PushLayoutState(*this);
}

LayoutState::LayoutState(LayoutObject& root)
    : containing_block_logical_width_changed_(false),
      next_(root.View()->GetLayoutState()),
      layout_object_(&root) {
  DCHECK(!next_);
  DCHECK(!IsA<LayoutView>(root));
  root.View()->PushLayoutState(*this);
}

LayoutState::~LayoutState() {
  if (layout_object_->View()->GetLayoutState()) {
    DCHECK_EQ(layout_object_->View()->GetLayoutState(), this);
    layout_object_->View()->PopLayoutState();
  }
}

void LayoutState::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_);
}

}  // namespace blink
