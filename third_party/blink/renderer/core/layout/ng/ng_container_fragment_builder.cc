// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

namespace {

bool IsInlineContainerForNode(const NGBlockNode& node,
                              const LayoutObject* inline_container) {
  return inline_container && inline_container->IsLayoutInline() &&
         inline_container->CanContainOutOfFlowPositionedElement(
             node.Style().GetPosition());
}

}  // namespace

void NGContainerFragmentBuilder::ReplaceChild(
    wtf_size_t index,
    const NGPhysicalContainerFragment& new_child,
    const LogicalOffset offset) {
  DCHECK_LT(index, children_.size());
  children_[index] = ChildWithOffset(offset, std::move(&new_child));
}

// Propagate data in |child| to this fragment. The |child| will then be added as
// a child fragment or a child fragment item.
void NGContainerFragmentBuilder::PropagateChildData(
    const NGPhysicalContainerFragment& child,
    const LogicalOffset& child_offset,
    const LayoutInline* inline_container) {
  // Collect the child's out of flow descendants.
  const WritingModeConverter converter(GetWritingDirection(), child.Size());
  for (const auto& descendant : child.OutOfFlowPositionedDescendants()) {
    NGLogicalStaticPosition static_position =
        descendant.static_position.ConvertToLogical(converter);
    static_position.offset += child_offset;

    const LayoutInline* new_inline_container = descendant.inline_container;
    if (!new_inline_container &&
        IsInlineContainerForNode(descendant.node, inline_container))
      new_inline_container = inline_container;

    // |oof_positioned_candidates_| should not have duplicated entries.
    DCHECK(std::none_of(
        oof_positioned_candidates_.begin(), oof_positioned_candidates_.end(),
        [&descendant](const NGLogicalOutOfFlowPositionedNode& node) {
          return node.node == descendant.node;
        }));
    oof_positioned_candidates_.emplace_back(descendant.node, static_position,
                                            new_inline_container);
  }

  if (const NGPhysicalBoxFragment* fragment =
          DynamicTo<NGPhysicalBoxFragment>(&child)) {
    if (fragment->HasOutOfFlowPositionedFragmentainerDescendants()) {
      const auto& out_of_flow_fragmentainer_descendants =
          fragment->OutOfFlowPositionedFragmentainerDescendants();
      const WritingModeConverter empty_outer_size(GetWritingDirection(),
                                                  PhysicalSize());
      for (const auto& descendant : out_of_flow_fragmentainer_descendants) {
        const NGPhysicalContainerFragment* containing_block_fragment =
            descendant.containing_block_fragment.get();
        if (!containing_block_fragment)
          containing_block_fragment = fragment;

        LogicalOffset containing_block_offset = converter.ToLogical(
            descendant.containing_block_offset, PhysicalSize());
        if (!child.IsFragmentainerBox())
          containing_block_offset.block_offset += child_offset.block_offset;
        if (IsBlockFragmentationContextRoot()) {
          containing_block_offset.block_offset +=
              fragmentainer_consumed_block_size_;
        }

        NGLogicalStaticPosition static_position =
            descendant.static_position.ConvertToLogical(empty_outer_size);
        oof_positioned_fragmentainer_descendants_.emplace_back(
            descendant.node, static_position, descendant.inline_container,
            /* needs_block_offset_adjustment */ false,
            containing_block_offset, containing_block_fragment);
      }
    }
  }

  // We only need to report if inflow or floating elements depend on the
  // percentage resolution block-size. OOF-positioned children resolve their
  // percentages against the "final" size of their parent.
  if (!has_descendant_that_depends_on_percentage_block_size_) {
    if (child.DependsOnPercentageBlockSize() && !child.IsOutOfFlowPositioned())
      has_descendant_that_depends_on_percentage_block_size_ = true;

    // We may have a child which has the following style:
    // <div style="position: relative; top: 50%;"></div>
    // We need to mark this as depending on our %-block-size for the its offset
    // to be correctly calculated. This is *slightly* too broad as it only
    // depends on the available block-size, rather than the %-block-size.
    const auto& child_style = child.Style();
    if (child.IsCSSBox() && child_style.GetPosition() == EPosition::kRelative) {
      if (IsHorizontalWritingMode(Style().GetWritingMode())) {
        if (child_style.Top().IsPercentOrCalc() ||
            child_style.Bottom().IsPercentOrCalc())
          has_descendant_that_depends_on_percentage_block_size_ = true;
      } else {
        if (child_style.Left().IsPercentOrCalc() ||
            child_style.Right().IsPercentOrCalc())
          has_descendant_that_depends_on_percentage_block_size_ = true;
      }
    }
  }

  // Compute |has_floating_descendants_for_paint_| to optimize tree traversal
  // in paint.
  if (!has_floating_descendants_for_paint_) {
    if (child.IsFloating() || child.IsLegacyLayoutRoot() ||
        (child.HasFloatingDescendantsForPaint() &&
         !child.IsPaintedAtomically()))
      has_floating_descendants_for_paint_ = true;
  }

  // The |has_adjoining_object_descendants_| is used to determine if a fragment
  // can be re-used when preceding floats are present.
  // If a fragment doesn't have any adjoining object descendants, and is
  // self-collapsing, it can be "shifted" anywhere.
  if (!has_adjoining_object_descendants_) {
    if (!child.IsFormattingContextRoot() &&
        child.HasAdjoiningObjectDescendants())
      has_adjoining_object_descendants_ = true;
  }

  // Collect any (block) break tokens, but skip break tokens for fragmentainers,
  // as they should only escape a fragmentation context at the discretion of the
  // fragmentation context.
  if (has_block_fragmentation_ && !child.IsFragmentainerBox()) {
    if (const NGBreakToken* child_break_token = child.BreakToken()) {
      switch (child.Type()) {
        case NGPhysicalFragment::kFragmentBox:
          child_break_tokens_.push_back(child_break_token);
          break;
        case NGPhysicalFragment::kFragmentLineBox:
          // NGInlineNode produces multiple line boxes in an anonymous box. We
          // won't know up front which line box to insert a fragment break
          // before (due to widows), so keep them all until we know.
          inline_break_tokens_.push_back(child_break_token);
          break;
        case NGPhysicalFragment::kFragmentText:
        default:
          NOTREACHED();
          break;
      }
    }
  }

  // Always store the consumed block size of the previous fragmentainer so the
  // out-of-flow positioned-nodes in the next fragmentainers can use it to
  // compute its start position.
  if (child.BreakToken() && child.IsFragmentainerBox()) {
    DCHECK(IsA<NGBlockBreakToken>(child.BreakToken()));
    fragmentainer_consumed_block_size_ =
        To<NGBlockBreakToken>(child.BreakToken())->ConsumedBlockSize();
  }
}

void NGContainerFragmentBuilder::AddChildInternal(
    scoped_refptr<const NGPhysicalFragment> child,
    const LogicalOffset& child_offset) {
  // In order to know where list-markers are within the children list (for the
  // |NGSimplifiedLayoutAlgorithm|) we always place them as the first child.
  if (child->IsListMarker()) {
    children_.push_front(ChildWithOffset(child_offset, std::move(child)));
    return;
  }

  children_.emplace_back(child_offset, std::move(child));
}

void NGContainerFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode child,
    const LogicalOffset& child_offset,
    NGLogicalStaticPosition::InlineEdge inline_edge,
    NGLogicalStaticPosition::BlockEdge block_edge,
    bool needs_block_offset_adjustment) {
  DCHECK(child);

  // If an OOF-positioned candidate has a static-position which uses a
  // non-block-start edge, we may need to adjust its static-position when the
  // final block-size is known.
  needs_block_offset_adjustment &=
      block_edge != NGLogicalStaticPosition::BlockEdge::kBlockStart;
  has_oof_candidate_that_needs_block_offset_adjustment_ |=
      needs_block_offset_adjustment;

  oof_positioned_candidates_.emplace_back(
      child, NGLogicalStaticPosition{child_offset, inline_edge, block_edge},
      /* inline_container */ nullptr, needs_block_offset_adjustment);
}

void NGContainerFragmentBuilder::AddOutOfFlowInlineChildCandidate(
    NGBlockNode child,
    const LogicalOffset& child_offset,
    TextDirection inline_container_direction) {
  DCHECK(node_.IsInline() || layout_object_->IsLayoutInline());

  // As all inline-level fragments are built in the line-logical coordinate
  // system (Direction() is kLtr), we need to know the direction of the
  // parent element to correctly determine an OOF childs static position.
  AddOutOfFlowChildCandidate(child, child_offset,
                             IsLtr(inline_container_direction)
                                 ? NGLogicalStaticPosition::kInlineStart
                                 : NGLogicalStaticPosition::kInlineEnd,
                             NGLogicalStaticPosition::kBlockStart);
}

void NGContainerFragmentBuilder::AddOutOfFlowFragmentainerDescendant(
    const NGLogicalOutOfFlowPositionedNode& descendant) {
  oof_positioned_fragmentainer_descendants_.push_back(descendant);
}

void NGContainerFragmentBuilder::AddOutOfFlowDescendant(
    const NGLogicalOutOfFlowPositionedNode& descendant) {
  oof_positioned_descendants_.push_back(descendant);
}

void NGContainerFragmentBuilder::SwapOutOfFlowPositionedCandidates(
    Vector<NGLogicalOutOfFlowPositionedNode>* candidates) {
  DCHECK(candidates->IsEmpty());
  std::swap(oof_positioned_candidates_, *candidates);

  if (!has_oof_candidate_that_needs_block_offset_adjustment_)
    return;

  using BlockEdge = NGLogicalStaticPosition::BlockEdge;

  // We might have an OOF-positioned candidate whose static-position depends on
  // the final block-size of this fragment.
  DCHECK_NE(BlockSize(), kIndefiniteSize);
  for (auto& candidate : *candidates) {
    if (!candidate.needs_block_offset_adjustment)
      continue;

    if (candidate.static_position.block_edge == BlockEdge::kBlockCenter)
      candidate.static_position.offset.block_offset += BlockSize() / 2;
    else if (candidate.static_position.block_edge == BlockEdge::kBlockEnd)
      candidate.static_position.offset.block_offset += BlockSize();
    candidate.needs_block_offset_adjustment = false;
  }

  has_oof_candidate_that_needs_block_offset_adjustment_ = false;
}

void NGContainerFragmentBuilder::SwapOutOfFlowFragmentainerDescendants(
    Vector<NGLogicalOutOfFlowPositionedNode>* descendants) {
  DCHECK(descendants->IsEmpty());
  DCHECK(!has_oof_candidate_that_needs_block_offset_adjustment_);
  std::swap(oof_positioned_fragmentainer_descendants_, *descendants);
}

void NGContainerFragmentBuilder::
    MoveOutOfFlowDescendantCandidatesToDescendants() {
  DCHECK(oof_positioned_descendants_.IsEmpty());
  std::swap(oof_positioned_candidates_, oof_positioned_descendants_);

  if (!layout_object_->IsInline())
    return;

  for (auto& candidate : oof_positioned_descendants_) {
    // If we are inside the inline algorithm, (and creating a fragment for a
    // <span> or similar), we may add a child (e.g. an atomic-inline) which has
    // OOF descandants.
    //
    // This checks if the object creating this box will be the container for
    // the given descendant.
    if (!candidate.inline_container &&
        IsInlineContainerForNode(candidate.node, layout_object_))
      candidate.inline_container = ToLayoutInline(layout_object_);

    // Ensure that the inline_container is a continuation root.
    if (candidate.inline_container) {
      candidate.inline_container =
          ToLayoutInline(candidate.inline_container->ContinuationRoot());
    }
  }
}

#if DCHECK_IS_ON()

String NGContainerFragmentBuilder::ToString() const {
  StringBuilder builder;
  builder.AppendFormat("ContainerFragment %.2fx%.2f, Children %u\n",
                       InlineSize().ToFloat(), BlockSize().ToFloat(),
                       children_.size());
  for (auto& child : children_) {
    builder.Append(child.fragment->DumpFragmentTree(
        NGPhysicalFragment::DumpAll & ~NGPhysicalFragment::DumpHeaderText));
  }
  return builder.ToString();
}

#endif

}  // namespace blink
