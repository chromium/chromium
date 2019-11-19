// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

template <typename Base>
LayoutNGBlockFlowMixin<Base>::LayoutNGBlockFlowMixin(Element* element)
    : LayoutNGMixin<Base>(element) {
  static_assert(std::is_base_of<LayoutBlockFlow, Base>::value,
                "Base class of LayoutNGBlockFlowMixin must be LayoutBlockFlow "
                "or derived class.");
  DCHECK(!element || !element->ShouldForceLegacyLayout());
}

template <typename Base>
LayoutNGBlockFlowMixin<Base>::~LayoutNGBlockFlowMixin() = default;

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  Base::StyleDidChange(diff, old_style);

  if (diff.NeedsCollectInlines()) {
    Base::SetNeedsCollectInlines();
  }
}

template <typename Base>
NGInlineNodeData* LayoutNGBlockFlowMixin<Base>::TakeNGInlineNodeData() {
  return ng_inline_node_data_.release();
}

template <typename Base>
NGInlineNodeData* LayoutNGBlockFlowMixin<Base>::GetNGInlineNodeData() const {
  DCHECK(ng_inline_node_data_);
  return ng_inline_node_data_.get();
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::ResetNGInlineNodeData() {
  ng_inline_node_data_ = std::make_unique<NGInlineNodeData>();
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::ClearNGInlineNodeData() {
  ng_inline_node_data_.reset();
}

// The current fragment from the last layout cycle for this box.
// When pre-NG layout calls functions of this block flow, fragment and/or
// LayoutResult are required to compute the result.
// TODO(kojii): Use the cached result for now, we may need to reconsider as the
// cache evolves.
template <typename Base>
const NGPhysicalBoxFragment* LayoutNGBlockFlowMixin<Base>::CurrentFragment()
    const {
  const NGLayoutResult* cached_layout_result = Base::GetCachedLayoutResult();
  if (!cached_layout_result)
    return nullptr;

  return &To<NGPhysicalBoxFragment>(cached_layout_result->PhysicalFragment());
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::AddLayoutOverflowFromChildren() {
  if (Base::LayoutBlockedByDisplayLock(DisplayLockLifecycleTarget::kChildren))
    return;

  // |ComputeOverflow()| calls this, which is called from
  // |CopyFragmentDataToLayoutBox()| and |RecalcOverflow()|.
  // Add overflow from the last layout cycle.
  // TODO(chrishtr): do we need to condition on CurrentFragment()? Why?
  if (CurrentFragment()) {
    AddScrollingOverflowFromChildren();
  }
  Base::AddLayoutOverflowFromChildren();
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::AddScrollingOverflowFromChildren() {

  const NGPhysicalBoxFragment* physical_fragment = CurrentFragment();
  DCHECK(physical_fragment);
  if (physical_fragment->Children().empty())
    return;

  const ComputedStyle& style = Base::StyleRef();
  const WritingMode writing_mode = style.GetWritingMode();
  const TextDirection direction = style.Direction();
  const LayoutUnit border_inline_start = LayoutUnit(style.BorderStartWidth());
  const LayoutUnit border_block_start = LayoutUnit(style.BorderBeforeWidth());
  const PhysicalSize& size = physical_fragment->Size();

  // End and under padding are added to scroll overflow of inline children.
  // https://github.com/w3c/csswg-drafts/issues/129
  base::Optional<NGPhysicalBoxStrut> padding_strut;
  if (Base::HasOverflowClip()) {
    padding_strut = NGBoxStrut(LayoutUnit(), Base::PaddingEnd(), LayoutUnit(),
                               Base::PaddingUnder())
                        .ConvertToPhysical(writing_mode, direction);
  }

  // Rectangles not reachable by scroll should not be added to overflow.
  auto IsRectReachableByScroll = [&border_inline_start, &border_block_start,
                                  &writing_mode, &direction,
                                  &size](const PhysicalRect& rect) {
    LogicalOffset rect_logical_end =
        rect.offset.ConvertToLogical(writing_mode, direction, size, rect.size) +
        rect.size.ConvertToLogical(writing_mode);
    return (rect_logical_end.inline_offset > border_inline_start &&
            rect_logical_end.block_offset > border_block_start);
  };

  bool children_inline = Base::ChildrenInline();
  PhysicalRect children_overflow;
  base::Optional<PhysicalRect> lineboxes_enclosing_rect;
  // Only add overflow for fragments NG has not reflected into Legacy.
  // These fragments are:
  // - inline fragments,
  // - out of flow fragments whose css container is inline box.
  // TODO(layout-dev) Transforms also need to be applied to compute overflow
  // correctly. NG is not yet transform-aware. crbug.com/855965
  for (const auto& child : physical_fragment->Children()) {
    PhysicalRect child_scrollable_overflow;
    if (child->IsFloatingOrOutOfFlowPositioned()) {
      child_scrollable_overflow = child->ScrollableOverflowForPropagation(this);
      child_scrollable_overflow.offset +=
          ComputeRelativeOffset(child->Style(), writing_mode, direction, size);
    } else if (children_inline && child->IsLineBox()) {
      DCHECK(child->IsLineBox());
      child_scrollable_overflow =
          To<NGPhysicalLineBoxFragment>(*child).ScrollableOverflow(this, &style,
                                                                   size);
      if (padding_strut) {
        PhysicalRect linebox_rect(child.Offset(), child->Size());
        if (lineboxes_enclosing_rect)
          lineboxes_enclosing_rect->Unite(linebox_rect);
        else
          lineboxes_enclosing_rect = linebox_rect;
      }
    } else {
      continue;
    }
    child_scrollable_overflow.offset += child.Offset();
    // Do not add overflow if fragment is not reachable by scrolling.
    if (IsRectReachableByScroll(child_scrollable_overflow))
      children_overflow.Unite(child_scrollable_overflow);
  }
  if (lineboxes_enclosing_rect) {
    lineboxes_enclosing_rect->Expand(*padding_strut);
    if (IsRectReachableByScroll(*lineboxes_enclosing_rect))
      children_overflow.Unite(*lineboxes_enclosing_rect);
  }

  // LayoutOverflow takes flipped blocks coordinates, adjust as needed.
  LayoutRect children_flipped_overflow =
      children_overflow.ToLayoutFlippedRect(style, size);
  Base::AddLayoutOverflow(children_flipped_overflow);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::AddOutlineRects(
    Vector<PhysicalRect>& rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  if (PaintFragment()) {
    To<NGPhysicalBoxFragment>(PaintFragment()->PhysicalFragment())
        .AddSelfOutlineRects(additional_offset, include_block_overflows,
                             &rects);
  } else {
    Base::AddOutlineRects(rects, additional_offset, include_block_overflows);
  }
}

template <typename Base>
bool LayoutNGBlockFlowMixin<
    Base>::PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
  // LayoutNGBlockFlowMixin is in charge of paint invalidation of the first
  // line.
  if (PaintFragment())
    return false;

  if (Base::StyleRef().HasColumnRule())
    return false;

  return Base::PaintedOutputOfObjectHasNoEffectRegardlessOfSize();
}

// Retrieve NGBaseline from the current fragment.
template <typename Base>
base::Optional<LayoutUnit> LayoutNGBlockFlowMixin<Base>::FragmentBaseline(
    NGBaselineAlgorithmType type) const {
  if (Base::ShouldApplyLayoutContainment())
    return base::nullopt;

  if (const NGPhysicalFragment* physical_fragment = CurrentFragment()) {
    FontBaseline baseline_type = Base::StyleRef().GetFontBaseline();
    return To<NGPhysicalBoxFragment>(physical_fragment)
        ->Baseline({type, baseline_type});
  }
  return base::nullopt;
}

template <typename Base>
LayoutUnit LayoutNGBlockFlowMixin<Base>::FirstLineBoxBaseline() const {
  if (Base::ChildrenInline()) {
    if (base::Optional<LayoutUnit> offset =
            FragmentBaseline(NGBaselineAlgorithmType::kFirstLine)) {
      return *offset;
    }
  }
  return Base::FirstLineBoxBaseline();
}

template <typename Base>
LayoutUnit LayoutNGBlockFlowMixin<Base>::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  if (Base::ChildrenInline()) {
    if (base::Optional<LayoutUnit> offset =
            FragmentBaseline(NGBaselineAlgorithmType::kAtomicInline)) {
      return *offset;
    }
  }
  return Base::InlineBlockBaseline(line_direction);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::SetPaintFragment(
    const NGBlockBreakToken* break_token,
    scoped_refptr<const NGPhysicalFragment> fragment) {
  DCHECK(!break_token || break_token->InputNode().GetLayoutBox() == this);

  if (UNLIKELY(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()))
    return;

  scoped_refptr<NGPaintFragment>* current =
      NGPaintFragment::Find(&paint_fragment_, break_token);
  DCHECK(current);
  if (fragment) {
    *current = NGPaintFragment::Create(std::move(fragment), break_token,
                                       std::move(*current));
    // |NGPaintFragment::Create()| calls |SlowSetPaintingLayerNeedsRepaint()|.
  } else if (*current) {
    DCHECK_EQ(this, (*current)->GetLayoutObject());
    // TODO(kojii): Pass break_token for LayoutObject that spans across block
    // fragmentation boundaries.
    (*current)->ClearAssociationWithLayoutObject();
    *current = nullptr;
    ObjectPaintInvalidator(*this).SlowSetPaintingLayerNeedsRepaint();
  }
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::Paint(const PaintInfo& paint_info) const {
  if (UNLIKELY(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())) {
    if (const NGPhysicalBoxFragment* fragment = CurrentFragment()) {
      if (fragment->HasItems()) {
        NGBoxFragmentPainter(*fragment).Paint(paint_info);
        return;
      }
    }
  }

  if (const NGPaintFragment* paint_fragment = PaintFragment()) {
    NGBoxFragmentPainter(*paint_fragment).Paint(paint_info);
    return;
  }

  if (RuntimeEnabledFeatures::LayoutNGFragmentPaintEnabled()) {
    if (const NGPhysicalBoxFragment* fragment = CurrentFragment()) {
      NGBoxFragmentPainter(*fragment).Paint(paint_info);
      return;
    }
  }

  Base::Paint(paint_info);
}

template <typename Base>
bool LayoutNGBlockFlowMixin<Base>::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestAction action) {
  const NGPaintFragment* paint_fragment = PaintFragment();
  if (!paint_fragment) {
    return LayoutBlockFlow::NodeAtPoint(result, hit_test_location,
                                        accumulated_offset, action);
  }

  if (!this->IsEffectiveRootScroller()) {
    // Check if we need to do anything at all.
    // If we have clipping, then we can't have any spillout.
    PhysicalRect overflow_box = Base::HasOverflowClip()
                                    ? Base::PhysicalBorderBoxRect()
                                    : Base::PhysicalVisualOverflowRect();
    overflow_box.Move(accumulated_offset);
    if (!hit_test_location.Intersects(overflow_box))
      return false;
  }
  if (Base::IsInSelfHitTestingPhase(action) && Base::HasOverflowClip() &&
      Base::HitTestOverflowControl(result, hit_test_location,
                                   accumulated_offset))
    return true;

  return NGBoxFragmentPainter(*paint_fragment)
      .NodeAtPoint(result, hit_test_location, accumulated_offset, action);
}

template <typename Base>
PositionWithAffinity LayoutNGBlockFlowMixin<Base>::PositionForPoint(
    const PhysicalOffset& point) const {
  if (Base::IsAtomicInlineLevel()) {
    const PositionWithAffinity atomic_inline_position =
        Base::PositionForPointIfOutsideAtomicInlineLevel(point);
    if (atomic_inline_position.IsNotNull())
      return atomic_inline_position;
  }

  if (!Base::ChildrenInline())
    return LayoutBlock::PositionForPoint(point);

  if (!PaintFragment())
    return Base::CreatePositionWithAffinity(0);

  // The given offset is relative to this |LayoutBlockFlow|. Convert to the
  // contents offset.
  PhysicalOffset point_in_contents = point;
  Base::OffsetForContents(point_in_contents);
  const PositionWithAffinity ng_position =
      PaintFragment()->PositionForPoint(point_in_contents);
  if (ng_position.IsNotNull())
    return ng_position;
  return Base::CreatePositionWithAffinity(0);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::DirtyLinesFromChangedChild(
    LayoutObject* child,
    MarkingBehavior marking_behavior) {
  DCHECK_EQ(marking_behavior, kMarkContainerChain);

  // We need to dirty line box fragments only if the child is once laid out in
  // LayoutNG inline formatting context. New objects are handled in
  // NGInlineNode::MarkLineBoxesDirty().
  if (child->IsInLayoutNGInlineFormattingContext())
    NGPaintFragment::DirtyLinesFromChangedChild(child);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::UpdateNGBlockLayout() {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (Base::IsOutOfFlowPositioned()) {
    this->UpdateOutOfFlowBlockLayout();
    return;
  }

  NGConstraintSpace constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(
          *this, !Base::View()->GetLayoutState()->Next() /* is_layout_root */);

  scoped_refptr<const NGLayoutResult> result =
      NGBlockNode(this).Layout(constraint_space);

  for (const auto& descendant :
       result->PhysicalFragment().OutOfFlowPositionedDescendants())
    descendant.node.UseLegacyOutOfFlowPositioning();
  this->UpdateMargins(constraint_space);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::UpdateMargins(
    const NGConstraintSpace& space) {
  const LayoutBlock* containing_block = Base::ContainingBlock();
  if (!containing_block || !containing_block->IsLayoutBlockFlow())
    return;

  // In the legacy engine, for regular block container layout, children
  // calculate and store margins on themselves, while in NG that's done by the
  // container. Since this object is a LayoutNG entry-point, we'll have to do it
  // on ourselves, since that's what the legacy container expects.
  const ComputedStyle& style = Base::StyleRef();
  const ComputedStyle& cb_style = containing_block->StyleRef();
  const auto writing_mode = cb_style.GetWritingMode();
  const auto direction = cb_style.Direction();
  LayoutUnit percentage_resolution_size =
      space.PercentageResolutionInlineSizeForParentWritingMode();
  NGBoxStrut margins = ComputePhysicalMargins(style, percentage_resolution_size)
                           .ConvertToLogical(writing_mode, direction);
  ResolveInlineMargins(style, cb_style, space.AvailableSize().inline_size,
                       Base::LogicalWidth(), &margins);
  this->SetMargin(margins.ConvertToPhysical(writing_mode, direction));
}

template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutBlockFlow>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutProgress>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutTableCaption>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutTableCell>;

}  // namespace blink
