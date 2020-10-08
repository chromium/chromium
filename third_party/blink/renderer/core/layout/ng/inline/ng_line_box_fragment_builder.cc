// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_logical_line_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

void NGLineBoxFragmentBuilder::Reset() {
  children_.Shrink(0);
  child_break_tokens_.Shrink(0);
  inline_break_tokens_.Shrink(0);
  oof_positioned_candidates_.Shrink(0);
  unpositioned_list_marker_ = NGUnpositionedListMarker();

  size_.inline_size = LayoutUnit();
  metrics_ = FontHeight::Empty();
  line_box_type_ = NGPhysicalLineBoxFragment::kNormalLineBox;

  break_appeal_ = kBreakAppealPerfect;
  has_floating_descendants_for_paint_ = false;
  has_descendant_that_depends_on_percentage_block_size_ = false;
  has_block_fragmentation_ = false;
}

void NGLineBoxFragmentBuilder::SetIsEmptyLineBox() {
  line_box_type_ = NGPhysicalLineBoxFragment::kEmptyLineBox;
}

void NGLineBoxFragmentBuilder::AddChild(
    const NGPhysicalContainerFragment& child,
    const LogicalOffset& child_offset) {
  PropagateChildData(child, child_offset);
  AddChildInternal(&child, child_offset);
}

void NGLineBoxFragmentBuilder::AddChildren(NGLogicalLineItems& children) {
  children_.ReserveCapacity(children.size());

  for (auto& child : children) {
    if (child.layout_result) {
      DCHECK(!child.text_fragment);
      AddChild(child.layout_result->PhysicalFragment(), child.Offset());
      child.layout_result.reset();
    } else if (child.text_fragment) {
      AddChild(std::move(child.text_fragment), child.Offset());
      DCHECK(!child.text_fragment);
    } else if (child.out_of_flow_positioned_box) {
      AddOutOfFlowInlineChildCandidate(
          NGBlockNode(ToLayoutBox(child.out_of_flow_positioned_box)),
          child.Offset(), child.container_direction);
      child.out_of_flow_positioned_box = nullptr;
    }
  }
}

void NGLineBoxFragmentBuilder::PropagateChildrenData(
    NGLogicalLineItems& children) {
  for (unsigned index = 0; index < children.size(); ++index) {
    auto& child = children[index];
    if (child.layout_result) {
      DCHECK(!child.text_fragment);
      PropagateChildData(child.layout_result->PhysicalFragment(),
                         child.Offset());

      // Skip over any children, the information should have already been
      // propagated into this layout result.
      if (child.children_count)
        index += child.children_count - 1;

      continue;
    }
    if (child.out_of_flow_positioned_box) {
      AddOutOfFlowInlineChildCandidate(
          NGBlockNode(ToLayoutBox(child.out_of_flow_positioned_box)),
          child.Offset(), child.container_direction);
      child.out_of_flow_positioned_box = nullptr;
    }
  }

  DCHECK(oof_positioned_descendants_.IsEmpty());
  MoveOutOfFlowDescendantCandidatesToDescendants();
}

scoped_refptr<const NGLayoutResult>
NGLineBoxFragmentBuilder::ToLineBoxFragment() {
  writing_direction_.SetWritingMode(ToLineWritingMode(GetWritingMode()));

  if (!break_token_)
    break_token_ = NGInlineBreakToken::Create(node_);

  scoped_refptr<const NGPhysicalLineBoxFragment> fragment =
      NGPhysicalLineBoxFragment::Create(this);

  return base::AdoptRef(
      new NGLayoutResult(NGLayoutResult::NGLineBoxFragmentBuilderPassKey(),
                         std::move(fragment), this));
}

}  // namespace blink
