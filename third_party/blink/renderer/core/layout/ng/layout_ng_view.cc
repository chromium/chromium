// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_view.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"

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
  relayout_children |=
      !ShouldUsePrintingLayout() &&
      (!GetFrameView() || LogicalWidth() != ViewLogicalWidthForBoxSizing() ||
       LogicalHeight() != ViewLogicalHeightForBoxSizing());
  if (relayout_children && GetDocument().SvgExtensions()) {
    GetDocument()
        .AccessSVGExtensions()
        .InvalidateSVGRootsWithRelativeLengthDescendents(nullptr);
  }

  NGConstraintSpace constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  NGBlockNode(this).Layout(constraint_space);
}

MinMaxSizes LayoutNGView::ComputeIntrinsicLogicalWidths() const {
  NOT_DESTROYED();
  WritingMode writing_mode = StyleRef().GetWritingMode();

  NGConstraintSpace space =
      NGConstraintSpaceBuilder(writing_mode, StyleRef().GetWritingDirection(),
                               /* is_new_fc */ true)
          .ToConstraintSpace();

  NGBlockNode node(const_cast<LayoutNGView*>(this));
  DCHECK(node.CanUseNewLayout());
  return node.ComputeMinMaxSizes(writing_mode, MinMaxSizesType::kContent, space)
      .sizes;
}

AtomicString LayoutNGView::NamedPageAtIndex(wtf_size_t page_index) const {
  // If LayoutNGView is enabled, but not LayoutNGPrinting, fall back to legacy.
  if (!RuntimeEnabledFeatures::LayoutNGPrintingEnabled())
    return LayoutView::NamedPageAtIndex(page_index);
  if (!PhysicalFragmentCount())
    return AtomicString();
  DCHECK_EQ(PhysicalFragmentCount(), 1u);
  const NGPhysicalBoxFragment& view_fragment = *GetPhysicalFragment(0);
  const auto children = view_fragment.Children();
  if (page_index >= children.size())
    return AtomicString();
  const auto& page_fragment = To<NGPhysicalBoxFragment>(*children[page_index]);
  return page_fragment.PageName();
}

}  // namespace blink
