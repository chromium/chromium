// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"

namespace blink {

void NGLineBoxFragmentBuilder::Reset() {
  children_.Shrink(0);
  child_break_tokens_.Shrink(0);
  last_inline_break_token_ = nullptr;
  oof_positioned_candidates_.Shrink(0);
  unpositioned_list_marker_ = NGUnpositionedListMarker();

  annotation_overflow_ = LayoutUnit();
  bfc_block_offset_.reset();
  line_box_bfc_block_offset_.reset();
  is_pushed_by_floats_ = false;
  subtree_modified_margin_strut_ = false;

  size_.inline_size = LayoutUnit();
  metrics_ = FontHeight::Empty();
  line_box_type_ = NGPhysicalLineBoxFragment::kNormalLineBox;

  has_floating_descendants_for_paint_ = false;
  has_descendant_that_depends_on_percentage_block_size_ = false;
  has_block_fragmentation_ = false;
}

void NGLineBoxFragmentBuilder::SetIsEmptyLineBox() {
  line_box_type_ = NGPhysicalLineBoxFragment::kEmptyLineBox;
}

void NGLineBoxFragmentBuilder::PropagateChildrenData(
    NGLogicalLineItems& children) {
  for (unsigned index = 0; index < children.size(); ++index) {
    auto& child = children[index];
    if (child.layout_result) {
      // An accumulated relative offset is applied to an OOF once it reaches its
      // inline container. Subtract out the relative offset to avoid adding it
      // twice.
      PropagateChildData(
          child.layout_result->PhysicalFragment(),
          child.Offset() -
              ComputeRelativeOffsetForInline(ConstraintSpace(),
                                             child.PhysicalFragment()->Style()),
          ComputeRelativeOffsetForOOFInInline(
              ConstraintSpace(), child.PhysicalFragment()->Style()));

      // Skip over any children, the information should have already been
      // propagated into this layout result.
      if (child.children_count)
        index += child.children_count - 1;

      continue;
    }
    if (child.out_of_flow_positioned_box) {
      AddOutOfFlowInlineChildCandidate(
          NGBlockNode(To<LayoutBox>(child.out_of_flow_positioned_box.Get())),
          child.Offset(), child.container_direction);
      child.out_of_flow_positioned_box = nullptr;
    }
  }

  DCHECK(oof_positioned_descendants_.empty());
  MoveOutOfFlowDescendantCandidatesToDescendants();
}

const NGLayoutResult* NGLineBoxFragmentBuilder::ToLineBoxFragment() {
  writing_direction_.SetWritingMode(ToLineWritingMode(GetWritingMode()));

  const NGPhysicalLineBoxFragment* fragment =
      NGPhysicalLineBoxFragment::Create(this);

  return MakeGarbageCollected<NGLayoutResult>(
      NGLayoutResult::NGLineBoxFragmentBuilderPassKey(), std::move(fragment),
      this);
}

}  // namespace blink
