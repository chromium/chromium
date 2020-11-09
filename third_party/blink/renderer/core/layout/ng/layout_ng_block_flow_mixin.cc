// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
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
}

template <typename Base>
LayoutNGBlockFlowMixin<Base>::~LayoutNGBlockFlowMixin() = default;

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  Base::StyleDidChange(diff, old_style);

  if (diff.NeedsReshape()) {
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

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::AddLayoutOverflowFromChildren() {
  if (Base::ChildLayoutBlockedByDisplayLock())
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
  PhysicalRect children_overflow =
      physical_fragment->ScrollableOverflowFromChildren(
          NGPhysicalFragment::kNormalHeight);

  // LayoutOverflow takes flipped blocks coordinates, adjust as needed.
  const ComputedStyle& style = physical_fragment->Style();
  LayoutRect children_flipped_overflow =
      children_overflow.ToLayoutFlippedRect(style, physical_fragment->Size());
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
    return;
  }

  // TODO(crbug.com/1145048): Currently |NGBoxPhysicalFragment| does not support
  // NG block fragmentation. Fallback to the legacy code path.
  if (Base::PhysicalFragmentCount() == 1) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    if (fragment->HasItems()) {
      fragment->AddSelfOutlineRects(additional_offset, include_block_overflows,
                                    &rects);
      return;
    }
  }

  Base::AddOutlineRects(rects, additional_offset, include_block_overflows);
}

template <typename Base>
LayoutUnit LayoutNGBlockFlowMixin<Base>::FirstLineBoxBaseline() const {
  // Please see |Paint()| for these DCHECKs.
  DCHECK(!Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  DCHECK_LE(Base::PhysicalFragmentCount(), 1u);

  if (const base::Optional<LayoutUnit> baseline =
          Base::FirstLineBoxBaselineOverride())
    return *baseline;

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    if (base::Optional<LayoutUnit> offset = fragment->Baseline())
      return *offset;
  }

  // This logic is in |LayoutBlock|, but we cannot call |Base| because doing so
  // may traverse |LayoutObject| tree, which may call this function for a child,
  // but the child may be block fragmented.
  if (Base::ChildrenInline()) {
    return Base::EmptyLineBaseline(
        Base::IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine);
  }
  return LayoutUnit(-1);
}

template <typename Base>
LayoutUnit LayoutNGBlockFlowMixin<Base>::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  // Please see |Paint()| for these DCHECKs.
  DCHECK(!Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  DCHECK_LE(Base::PhysicalFragmentCount(), 1u);

  if (const base::Optional<LayoutUnit> baseline =
          Base::InlineBlockBaselineOverride(line_direction))
    return *baseline;

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    if (base::Optional<LayoutUnit> offset = fragment->Baseline())
      return *offset;
  }

  // This logic is in |LayoutBlock|, but we cannot call |Base| because doing so
  // may traverse |LayoutObject| tree, which may call this function for a child,
  // but the child may be block fragmented.
  for (LayoutBox* child = Base::LastChildBox(); child;
       child = child->PreviousSiblingBox()) {
    if (!child->IsFloatingOrOutOfFlowPositioned())
      return LayoutUnit(-1);
  }
  return Base::EmptyLineBaseline(line_direction);
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
  // Avoid painting dirty objects because descendants maybe already destroyed.
  if (UNLIKELY(Base::NeedsLayout() &&
               !Base::ChildLayoutBlockedByDisplayLock())) {
    NOTREACHED();
    return;
  }

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

  if (const NGPhysicalBoxFragment* fragment = CurrentFragment()) {
    NGBoxFragmentPainter(*fragment).Paint(paint_info);
    return;
  }

  Base::Paint(paint_info);
}

template <typename Base>
bool LayoutNGBlockFlowMixin<Base>::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestAction action) {
  // When |this| is NG block fragmented, the painter should traverse fragemnts
  // instead of |LayoutObject|, because this function cannot handle block
  // fragmented objects. We can come here only when |this| cannot traverse
  // fragments, or the parent is legacy.
  DCHECK(!Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  DCHECK_LE(Base::PhysicalFragmentCount(), 1u);

  if (!Base::MayIntersect(result, hit_test_location, accumulated_offset))
    return false;

  if (const NGPaintFragment* paint_fragment = PaintFragment()) {
    if (Base::IsInSelfHitTestingPhase(action) && Base::IsScrollContainer() &&
        Base::HitTestOverflowControl(result, hit_test_location,
                                     accumulated_offset))
      return true;

    return NGBoxFragmentPainter(*paint_fragment)
        .NodeAtPoint(result, hit_test_location, accumulated_offset, action);
  }

  if (UNLIKELY(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())) {
    if (Base::PhysicalFragmentCount()) {
      const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
      DCHECK(fragment);
      if (fragment->HasItems() ||
          // Check descendants of this fragment because floats may be in the
          // |NGFragmentItems| of the descendants.
          (action == kHitTestFloat &&
           fragment->HasFloatingDescendantsForPaint())) {
        return NGBoxFragmentPainter(*fragment).NodeAtPoint(
            result, hit_test_location, accumulated_offset, action);
      }
    }
  }

  return LayoutBlockFlow::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, action);
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

  if (const NGPaintFragment* paint_fragment = PaintFragment()) {
    // The given offset is relative to this |LayoutBlockFlow|. Convert to the
    // contents offset.
    PhysicalOffset point_in_contents = point;
    Base::OffsetForContents(point_in_contents);
    if (const PositionWithAffinity position =
            paint_fragment->PositionForPoint(point_in_contents))
      return AdjustForEditingBoundary(position);
  } else if (const NGPhysicalBoxFragment* fragment = CurrentFragment()) {
    return fragment->PositionForPoint(point);
  }

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
  if (child->IsInLayoutNGInlineFormattingContext() &&
      RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    NGFragmentItems::DirtyLinesFromChangedChild(*child, *this);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::UpdateNGBlockLayout() {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (Base::IsOutOfFlowPositioned()) {
    LayoutNGMixin<Base>::UpdateOutOfFlowBlockLayout();
    return;
  }

  LayoutNGMixin<Base>::UpdateInFlowBlockLayout();
  UpdateMargins();
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::UpdateMargins() {
  const LayoutBlock* containing_block = Base::ContainingBlock();
  if (!containing_block || !containing_block->IsLayoutBlockFlow())
    return;

  // In the legacy engine, for regular block container layout, children
  // calculate and store margins on themselves, while in NG that's done by the
  // container. Since this object is a LayoutNG entry-point, we'll have to do it
  // on ourselves, since that's what the legacy container expects.
  const ComputedStyle& style = Base::StyleRef();
  const ComputedStyle& cb_style = containing_block->StyleRef();
  const auto writing_direction = cb_style.GetWritingDirection();
  LayoutUnit available_logical_width =
      LayoutBoxUtils::AvailableLogicalWidth(*this, containing_block);
  NGBoxStrut margins = ComputePhysicalMargins(style, available_logical_width)
                           .ConvertToLogical(writing_direction);
  ResolveInlineMargins(style, cb_style, available_logical_width,
                       Base::LogicalWidth(), &margins);
  Base::SetMargin(margins.ConvertToPhysical(writing_direction));
}

template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutBlockFlow>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutProgress>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutRubyAsBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutRubyBase>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutRubyRun>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutRubyText>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutTableCaption>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutTableCell>;

}  // namespace blink
