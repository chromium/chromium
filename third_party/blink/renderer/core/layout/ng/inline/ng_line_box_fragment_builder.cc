// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
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
  metrics_ = NGLineHeightMetrics();
  line_box_type_ = NGPhysicalLineBoxFragment::kNormalLineBox;

  break_appeal_ = kBreakAppealPerfect;
  has_floating_descendants_for_paint_ = false;
  has_orthogonal_flow_roots_ = false;
  has_descendant_that_depends_on_percentage_block_size_ = false;
  has_block_fragmentation_ = false;
  may_have_descendant_above_block_start_ = false;
}

void NGLineBoxFragmentBuilder::SetIsEmptyLineBox() {
  line_box_type_ = NGPhysicalLineBoxFragment::kEmptyLineBox;
}

NGLineBoxFragmentBuilder::Child*
NGLineBoxFragmentBuilder::ChildList::FirstInFlowChild() {
  for (auto& child : *this) {
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

NGLineBoxFragmentBuilder::Child*
NGLineBoxFragmentBuilder::ChildList::LastInFlowChild() {
  for (auto it = rbegin(); it != rend(); it++) {
    auto& child = *it;
    if (child.HasInFlowFragment())
      return &child;
  }
  return nullptr;
}

void NGLineBoxFragmentBuilder::ChildList::InsertChild(unsigned index) {
  children_.insert(index, Child());
}

void NGLineBoxFragmentBuilder::ChildList::MoveInInlineDirection(
    LayoutUnit delta) {
  for (auto& child : children_)
    child.offset.inline_offset += delta;
}

void NGLineBoxFragmentBuilder::ChildList::MoveInInlineDirection(
    LayoutUnit delta,
    unsigned start,
    unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].offset.inline_offset += delta;
}

void NGLineBoxFragmentBuilder::ChildList::MoveInBlockDirection(
    LayoutUnit delta) {
  for (auto& child : children_)
    child.offset.block_offset += delta;
}

void NGLineBoxFragmentBuilder::ChildList::MoveInBlockDirection(LayoutUnit delta,
                                                               unsigned start,
                                                               unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].offset.block_offset += delta;
}

void NGLineBoxFragmentBuilder::AddChildren(ChildList& children) {
  children_.ReserveCapacity(children.size());

  for (auto& child : children) {
    if (child.layout_result) {
      DCHECK(!child.fragment);
      AddChild(child.layout_result->PhysicalFragment(), child.offset);
      child.layout_result.reset();
    } else if (child.fragment) {
      AddChild(std::move(child.fragment), child.offset);
      DCHECK(!child.fragment);
    } else if (child.out_of_flow_positioned_box) {
      AddOutOfFlowChildCandidate(
          NGBlockNode(ToLayoutBox(child.out_of_flow_positioned_box)),
          child.offset, child.container_direction);
      child.out_of_flow_positioned_box = nullptr;
    }
  }
}

void NGLineBoxFragmentBuilder::PropagateChildrenData(ChildList& children) {
  for (auto& child : children) {
    if (child.layout_result) {
      DCHECK(!child.fragment);
      PropagateChildData(child.layout_result->PhysicalFragment(), child.offset);
    } else if (child.out_of_flow_positioned_box) {
      AddOutOfFlowChildCandidate(
          NGBlockNode(ToLayoutBox(child.out_of_flow_positioned_box)),
          child.offset, child.container_direction);
      child.out_of_flow_positioned_box = nullptr;
    }
  }

  DCHECK(oof_positioned_descendants_.IsEmpty());
  MoveOutOfFlowDescendantCandidatesToDescendants();
}

scoped_refptr<const NGLayoutResult>
NGLineBoxFragmentBuilder::ToLineBoxFragment() {
  writing_mode_ = ToLineWritingMode(writing_mode_);

  if (!break_token_)
    break_token_ = NGInlineBreakToken::Create(node_);

  scoped_refptr<const NGPhysicalLineBoxFragment> fragment =
      NGPhysicalLineBoxFragment::Create(this);

  return base::AdoptRef(new NGLayoutResult(std::move(fragment), this));
}

}  // namespace blink
