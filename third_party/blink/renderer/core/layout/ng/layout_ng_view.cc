// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_view.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

namespace blink {

LayoutNGView::LayoutNGView(ContainerNode* document)
    : LayoutNGBlockFlowMixin<LayoutView>(document) {
  DCHECK(document->IsDocumentNode());

  // This flag is normally set when an object is inserted into the tree, but
  // this doesn't happen for LayoutNGView, since it's the root.
  SetMightTraversePhysicalFragments(true);
}

LayoutNGView::~LayoutNGView() = default;

bool LayoutNGView::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGView ||
         LayoutNGMixin<LayoutView>::IsOfType(type);
}

bool LayoutNGView::IsFragmentationContextRoot() const {
  return ShouldUsePrintingLayout();
}

void LayoutNGView::UpdateBlockLayout(bool relayout_children) {
  NGConstraintSpace constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  NGBlockNode(this).Layout(constraint_space);
}

}  // namespace blink
