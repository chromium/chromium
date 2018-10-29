// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

NGContainerFragmentBuilder& NGContainerFragmentBuilder::AddChild(
    const NGLayoutResult& child,
    const NGLogicalOffset& child_offset) {
  // Collect the child's out of flow descendants.
  // child_offset is offset of inline_start/block_start vertex.
  // Candidates need offset of top/left vertex.
  const auto& out_of_flow_descendants = child.OutOfFlowPositionedDescendants();
  if (!out_of_flow_descendants.IsEmpty()) {
    NGLogicalOffset top_left_offset;
    NGPhysicalSize child_size = child.PhysicalFragment()->Size();
    switch (GetWritingMode()) {
      case WritingMode::kHorizontalTb:
        top_left_offset =
            (IsRtl(Direction()))
                ? NGLogicalOffset{child_offset.inline_offset + child_size.width,
                                  child_offset.block_offset}
                : child_offset;
        break;
      case WritingMode::kVerticalRl:
      case WritingMode::kSidewaysRl:
        top_left_offset =
            (IsRtl(Direction()))
                ? NGLogicalOffset{child_offset.inline_offset +
                                      child_size.height,
                                  child_offset.block_offset + child_size.width}
                : NGLogicalOffset{child_offset.inline_offset,
                                  child_offset.block_offset + child_size.width};
        break;
      case WritingMode::kVerticalLr:
      case WritingMode::kSidewaysLr:
        top_left_offset = (IsRtl(Direction()))
                              ? NGLogicalOffset{child_offset.inline_offset +
                                                    child_size.height,
                                                child_offset.block_offset}
                              : child_offset;
        break;
    }
    for (const NGOutOfFlowPositionedDescendant& descendant :
         out_of_flow_descendants) {
      oof_positioned_candidates_.push_back(
          NGOutOfFlowPositionedCandidate{descendant, top_left_offset});
    }
  }

  return AddChild(child.PhysicalFragment(), child_offset);
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::AddChild(
    scoped_refptr<const NGPhysicalFragment> child,
    const NGLogicalOffset& child_offset) {
  if (!has_last_resort_break_) {
    if (const auto* token = child->BreakToken()) {
      if (token->IsBlockType() &&
          ToNGBlockBreakToken(token)->HasLastResortBreak())
        has_last_resort_break_ = true;
    }
  }
  children_.emplace_back(std::move(child));
  offsets_.push_back(child_offset);
  return *this;
}

NGContainerFragmentBuilder&
NGContainerFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode child,
    const NGLogicalOffset& child_offset) {
  DCHECK(child);
  oof_positioned_candidates_.push_back(NGOutOfFlowPositionedCandidate(
      NGOutOfFlowPositionedDescendant{
          child, NGStaticPosition::Create(GetWritingMode(), Direction(),
                                          NGPhysicalOffset())},
      child_offset));

  return *this;
}

NGContainerFragmentBuilder&
NGContainerFragmentBuilder::AddInlineOutOfFlowChildCandidate(
    NGBlockNode child,
    const NGLogicalOffset& child_offset,
    TextDirection line_direction,
    LayoutObject* inline_container) {
  DCHECK(child);
  // Fixed positioned children are never placed inside inline container.
  if (child.Style().GetPosition() == EPosition::kFixed)
    inline_container = nullptr;
  oof_positioned_candidates_.push_back(NGOutOfFlowPositionedCandidate(
      NGOutOfFlowPositionedDescendant(
          child,
          NGStaticPosition::Create(GetWritingMode(), line_direction,
                                   NGPhysicalOffset()),
          inline_container),
      child_offset, line_direction));

  return *this;
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::AddOutOfFlowDescendant(
    NGOutOfFlowPositionedDescendant descendant) {
  oof_positioned_descendants_.push_back(descendant);
  return *this;
}

void NGContainerFragmentBuilder::GetAndClearOutOfFlowDescendantCandidates(
    Vector<NGOutOfFlowPositionedDescendant>* descendant_candidates,
    const LayoutObject* current_container) {
  DCHECK(descendant_candidates->IsEmpty());

  if (oof_positioned_candidates_.size() == 0)
    return;

  descendant_candidates->ReserveCapacity(oof_positioned_candidates_.size());

  DCHECK_GE(InlineSize(), LayoutUnit());
  DCHECK_GE(BlockSize(), LayoutUnit());
  NGPhysicalSize builder_physical_size{
      Size().ConvertToPhysical(GetWritingMode())};

  for (NGOutOfFlowPositionedCandidate& candidate : oof_positioned_candidates_) {
    TextDirection direction =
        candidate.is_line_relative ? candidate.line_direction : Direction();
    NGPhysicalOffset child_offset = candidate.child_offset.ConvertToPhysical(
        GetWritingMode(), direction, builder_physical_size, NGPhysicalSize());

    NGStaticPosition builder_relative_position;
    builder_relative_position.type = candidate.descendant.static_position.type;
    builder_relative_position.offset =
        child_offset + candidate.descendant.static_position.offset;

    descendant_candidates->push_back(NGOutOfFlowPositionedDescendant(
        candidate.descendant.node, builder_relative_position,
        candidate.descendant.inline_container));
    NGLogicalOffset container_offset =
        builder_relative_position.offset.ConvertToLogical(
            GetWritingMode(), Direction(), builder_physical_size,
            NGPhysicalSize());
    candidate.descendant.node.SaveStaticOffsetForLegacy(container_offset,
                                                        current_container);
  }

  // Clear our current canidate list. This may get modified again if the
  // current fragment is a containing block, and AddChild is called with a
  // descendant from this list.
  //
  // The descendant may be a "position: absolute" which contains a "position:
  // fixed" for example. (This fragment isn't the containing block for the
  // fixed descendant).
  oof_positioned_candidates_.Shrink(0);
}

void NGContainerFragmentBuilder::MoveOutOfFlowDescendantCandidatesToDescendants(
    const LayoutObject* inline_container) {
  GetAndClearOutOfFlowDescendantCandidates(&oof_positioned_descendants_,
                                           nullptr);
  if (inline_container) {
    for (auto& descendant : oof_positioned_descendants_) {
      if (!descendant.inline_container)
        descendant.inline_container = inline_container;
    }
  }
}

#ifndef NDEBUG

String NGContainerFragmentBuilder::ToString() const {
  StringBuilder builder;
  builder.Append(String::Format("ContainerFragment %.2fx%.2f, Children %u\n",
                                InlineSize().ToFloat(), BlockSize().ToFloat(),
                                children_.size()));
  for (auto& child : children_) {
    builder.Append(child->DumpFragmentTree(
        NGPhysicalFragment::DumpAll & ~NGPhysicalFragment::DumpHeaderText));
  }
  return builder.ToString();
}

#endif

}  // namespace blink
