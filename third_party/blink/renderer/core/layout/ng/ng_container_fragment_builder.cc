// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
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

void NGContainerFragmentBuilder::AddChild(
    const NGPhysicalContainerFragment& child,
    const LogicalOffset& child_offset,
    const LayoutInline* inline_container) {
  PropagateChildData(child, child_offset, inline_container);
  AddChildInternal(&child, child_offset);
}

// Propagate data in |child| to this fragment. The |child| will then be added as
// a child fragment or a child fragment item.
void NGContainerFragmentBuilder::PropagateChildData(
    const NGPhysicalContainerFragment& child,
    const LogicalOffset& child_offset,
    const LayoutInline* inline_container) {
  // Collect the child's out of flow descendants.
  // child_offset is offset of inline_start/block_start vertex.
  // Candidates need offset of top/left vertex.
  if (child.HasOutOfFlowPositionedDescendants()) {
    const auto& out_of_flow_descendants =
        child.OutOfFlowPositionedDescendants();
    PhysicalSize child_size = child.Size();

    // We can end up in a case where we need to account for the relative
    // position of an element to correctly determine the static position of a
    // descendant. E.g.
    // <div id="fixed_container">
    //   <div style="position: relative; top: 10px;">
    //     <div style="position: fixed;"></div>
    //   </div>
    // </div>
    // TODO(layout-dev): This code should eventually be removed once we handle
    // relative positioned objects directly in the fragment tree.
    LogicalOffset offset = child_offset;
    if (const LayoutBox* child_box =
            ToLayoutBoxOrNull(child.GetLayoutObject())) {
      offset += PhysicalOffset(child_box->OffsetForInFlowPosition())
                    .ConvertToLogical(GetWritingMode(), Direction(),
                                      PhysicalSize(), PhysicalSize());
    }

    for (const auto& descendant : out_of_flow_descendants) {
      NGLogicalStaticPosition static_position =
          descendant.static_position.ConvertToLogical(GetWritingMode(),
                                                      Direction(), child_size);
      static_position.offset += offset;

      const LayoutInline* new_inline_container = descendant.inline_container;
      if (!descendant.inline_container &&
          IsInlineContainerForNode(descendant.node, inline_container))
        new_inline_container = inline_container;

      oof_positioned_candidates_.emplace_back(descendant.node, static_position,
                                              new_inline_container);
    }
  }

  // For the |has_orthogonal_flow_roots_| flag, we don't care about the type of
  // child (OOF-positioned, etc), it is for *any* descendant.
  if (child.HasOrthogonalFlowRoots() ||
      !IsParallelWritingMode(child.Style().GetWritingMode(),
                             Style().GetWritingMode()))
    has_orthogonal_flow_roots_ = true;

  // We only need to report if inflow or floating elements depend on the
  // percentage resolution block-size. OOF-positioned children resolve their
  // percentages against the "final" size of their parent.
  if (child.DependsOnPercentageBlockSize() && !child.IsOutOfFlowPositioned())
    has_descendant_that_depends_on_percentage_block_size_ = true;

  // The |may_have_descendant_above_block_start_| flag is used to determine if
  // a fragment can be re-used when preceding floats are present. This is
  // relatively rare, and is true if:
  //  - An inflow child is positioned above our block-start edge.
  //  - Any inflow descendants (within the same formatting-context) which *may*
  //    have a child positioned above our block-start edge.
  if ((child_offset.block_offset < LayoutUnit() &&
       !child.IsOutOfFlowPositioned()) ||
      (!child.IsBlockFormattingContextRoot() && !child.IsLineBox() &&
       child.MayHaveDescendantAboveBlockStart()))
    may_have_descendant_above_block_start_ = true;

  // Compute |has_floating_descendants_for_paint_| to optimize tree traversal
  // in paint.
  if (!has_floating_descendants_for_paint_) {
    // TODO(layout-dev): The |NGPhysicalFragment::IsAtomicInline| check should
    // be checking for any children which paint all phases atomically.
    if (child.IsFloating() || child.IsLegacyLayoutRoot() ||
        (child.HasFloatingDescendantsForPaint() && !child.IsAtomicInline()))
      has_floating_descendants_for_paint_ = true;
  }

  // The |has_adjoining_object_descendants_| is used to determine if a fragment
  // can be re-used when preceding floats are present.
  // If a fragment doesn't have any adjoining object descendants, and is
  // self-collapsing, it can be "shifted" anywhere.
  if (!has_adjoining_object_descendants_) {
    if (!child.IsBlockFormattingContextRoot() &&
        child.HasAdjoiningObjectDescendants())
      has_adjoining_object_descendants_ = true;
  }

  // Collect any (block) break tokens, unless this is a fragmentation context
  // root. Break tokens should only escape a fragmentation context at the
  // discretion of the fragmentation context.
  if (has_block_fragmentation_ && !is_fragmentation_context_root_) {
    if (const NGBreakToken* child_break_token = child.BreakToken()) {
      switch (child.Type()) {
        case NGPhysicalFragment::kFragmentBox:
        case NGPhysicalFragment::kFragmentRenderedLegend:
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

LogicalOffset NGContainerFragmentBuilder::GetChildOffset(
    const LayoutObject* object) const {
  for (const auto& child : children_) {
    if (child.fragment->GetLayoutObject() == object)
      return child.offset;

    // TODO(layout-dev): ikilpatrick thinks we may need to traverse
    // further than the initial line-box children for a nested inline
    // container. We could not come up with a testcase, it would be
    // something with split inlines, and nested oof/fixed descendants maybe.
    if (child.fragment->IsLineBox()) {
      const auto& line_box_fragment =
          To<NGPhysicalLineBoxFragment>(*child.fragment);
      for (const auto& line_box_child : line_box_fragment.Children()) {
        if (line_box_child->GetLayoutObject() == object) {
          return child.offset + line_box_child.Offset().ConvertToLogical(
                                    GetWritingMode(), Direction(),
                                    line_box_fragment.Size(),
                                    line_box_child->Size());
        }
      }
    }
  }
  NOTREACHED();
  return LogicalOffset();
}

void NGContainerFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode child,
    const LogicalOffset& child_offset,
    base::Optional<TextDirection> container_direction) {
  DCHECK(child);
  DCHECK(layout_object_ && !layout_object_->IsLayoutInline() ||
         container_direction)
      << "container_direction must only be set for inline-level OOF children.";

  // As all inline-level fragments are built in the line-logical coordinate
  // system (Direction() is kLtr), we need to know the direction of the
  // parent element to correctly determine an OOF childs static position.
  TextDirection direction = container_direction.value_or(TextDirection::kLtr);

  oof_positioned_candidates_.emplace_back(
      child,
      NGLogicalStaticPosition{
          child_offset,
          IsLtr(direction) ? NGLogicalStaticPosition::InlineEdge::kInlineStart
                           : NGLogicalStaticPosition::InlineEdge::kInlineEnd,
          NGLogicalStaticPosition::BlockEdge::kBlockStart});
}

void NGContainerFragmentBuilder::AddOutOfFlowDescendant(
    const NGLogicalOutOfFlowPositionedNode& descendant) {
  oof_positioned_descendants_.push_back(descendant);
}

void NGContainerFragmentBuilder::SwapOutOfFlowPositionedCandidates(
    Vector<NGLogicalOutOfFlowPositionedNode>* candidates,
    const LayoutObject* current_container) {
  DCHECK(candidates->IsEmpty());

  // The |oof_positioned_candidates_| list may get additional candidates, in the
  // following case:
  //  - If an absolute-positioned element gets added to the builder, and it has
  //    a fixed-positioned descendant.
  std::swap(oof_positioned_candidates_, *candidates);

  for (auto& candidate : *candidates) {
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

    // Copy-back the static-position for legacy.
    candidate.node.SaveStaticOffsetForLegacy(candidate.static_position.offset,
                                             current_container);
  }

  DCHECK(oof_positioned_candidates_.IsEmpty());
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
