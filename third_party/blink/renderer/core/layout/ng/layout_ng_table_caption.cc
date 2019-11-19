// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_table_caption.h"

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

LayoutNGTableCaption::LayoutNGTableCaption(Element* element)
    : LayoutNGBlockFlowMixin<LayoutTableCaption>(element) {}

void LayoutNGTableCaption::CalculateAndSetMargins(
    const NGConstraintSpace& constraint_space,
    const NGPhysicalFragment& physical_fragment) {
  const ComputedStyle& containing_block_style = ContainingBlock()->StyleRef();

  NGBoxFragment box_fragment(containing_block_style.GetWritingMode(),
                             containing_block_style.Direction(),
                             To<NGPhysicalBoxFragment>(physical_fragment));

  NGPhysicalBoxStrut physical_margins =
      ComputePhysicalMargins(constraint_space, StyleRef());

  NGBoxStrut logical_margins =
      physical_margins.ConvertToLogical(containing_block_style.GetWritingMode(),
                                        containing_block_style.Direction());

  LayoutUnit caption_inline_size_in_cb_writing_mode = box_fragment.InlineSize();

  LayoutUnit available_inline_size_in_cb_writing_mode =
      ToPhysicalSize(constraint_space.AvailableSize(),
                     constraint_space.GetWritingMode())
          .ConvertToLogical(containing_block_style.GetWritingMode())
          .inline_size;

  ResolveInlineMargins(StyleRef(), containing_block_style,
                       available_inline_size_in_cb_writing_mode,
                       caption_inline_size_in_cb_writing_mode,
                       &logical_margins);
  SetMargin(
      logical_margins.ConvertToPhysical(containing_block_style.GetWritingMode(),
                                        containing_block_style.Direction()));
}

void LayoutNGTableCaption::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  DCHECK(!IsOutOfFlowPositioned()) << "Out of flow captions are blockified.";

  NGConstraintSpace constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(
          *this, !View()->GetLayoutState()->Next() /* is_layout_root */);

  scoped_refptr<const NGLayoutResult> result =
      NGBlockNode(this).Layout(constraint_space);

  CalculateAndSetMargins(constraint_space, result->PhysicalFragment());

  // Tell legacy layout there were abspos descendents we couldn't place. We know
  // we have to pass up to legacy here because this method is legacy's entry
  // point to LayoutNG. If our parent were LayoutNG, it wouldn't have called
  // UpdateBlockLayout, it would have packaged this LayoutObject into
  // NGBlockNode and called Layout on that.
  for (const auto& descendant :
       result->PhysicalFragment().OutOfFlowPositionedDescendants())
    descendant.node.UseLegacyOutOfFlowPositioning();

  // The parent table sometimes changes the caption's position after laying it
  // out. So there's no point in setting the fragment's offset here;
  // NGBoxFragmentPainter::Paint will have to handle it until table layout is
  // implemented in NG, in which case that algorithm will set each child's
  // offsets. See https://crbug.com/788590 for more info.
  DCHECK(!result->PhysicalFragment().IsPlacedByLayoutNG())
      << "Only a table should be placing table caption fragments and the ng "
         "table algorithm doesn't exist yet!";
}

}  // namespace blink
