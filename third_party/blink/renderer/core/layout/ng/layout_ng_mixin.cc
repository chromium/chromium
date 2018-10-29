// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/paint/ng/ng_block_flow_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

template <typename Base>
LayoutNGMixin<Base>::LayoutNGMixin(Element* element) : Base(element) {
  static_assert(
      std::is_base_of<LayoutBlockFlow, Base>::value,
      "Base class of LayoutNGMixin must be LayoutBlockFlow or derived class.");
}

template <typename Base>
LayoutNGMixin<Base>::~LayoutNGMixin() = default;

template <typename Base>
bool LayoutNGMixin<Base>::IsOfType(LayoutObject::LayoutObjectType type) const {
  return type == LayoutObject::kLayoutObjectNGMixin || Base::IsOfType(type);
}

template <typename Base>
NGInlineNodeData* LayoutNGMixin<Base>::TakeNGInlineNodeData() {
  return ng_inline_node_data_.release();
}

template <typename Base>
NGInlineNodeData* LayoutNGMixin<Base>::GetNGInlineNodeData() const {
  DCHECK(ng_inline_node_data_);
  return ng_inline_node_data_.get();
}

template <typename Base>
void LayoutNGMixin<Base>::ResetNGInlineNodeData() {
  ng_inline_node_data_ = std::make_unique<NGInlineNodeData>();
}

// The current fragment from the last layout cycle for this box.
// When pre-NG layout calls functions of this block flow, fragment and/or
// LayoutResult are required to compute the result.
// TODO(kojii): Use the cached result for now, we may need to reconsider as the
// cache evolves.
template <typename Base>
const NGPhysicalBoxFragment* LayoutNGMixin<Base>::CurrentFragment() const {
  if (cached_result_)
    return ToNGPhysicalBoxFragment(cached_result_->PhysicalFragment().get());
  return nullptr;
}

template <typename Base>
void LayoutNGMixin<Base>::ComputeVisualOverflow(
    const LayoutRect& previous_visual_overflow_rect,
    bool recompute_floats) {
  Base::ComputeVisualOverflow(previous_visual_overflow_rect, recompute_floats);
  AddVisualOverflowFromChildren();

  if (Base::VisualOverflowRect() != previous_visual_overflow_rect) {
    if (Base::Layer())
      Base::Layer()->SetNeedsCompositingInputsUpdate();
    Base::GetFrameView()->SetIntersectionObservationState(
        LocalFrameView::kDesired);
  }
}

template <typename Base>
void LayoutNGMixin<Base>::AddVisualOverflowFromChildren() {
  // |ComputeOverflow()| calls this, which is called from
  // |CopyFragmentDataToLayoutBox()| and |RecalcOverflowAfterStyleChange()|.
  // Add overflow from the last layout cycle.
  if (const NGPhysicalBoxFragment* physical_fragment = CurrentFragment()) {
    if (Base::ChildrenInline()) {
      Base::AddSelfVisualOverflow(
          physical_fragment->SelfInkOverflow().ToLayoutFlippedRect(
              physical_fragment->Style(), physical_fragment->Size()));
      // TODO(kojii): If |RecalcOverflowAfterStyleChange()|, we need to
      // re-compute glyph bounding box. How to detect it and how to re-compute
      // is TBD.
      Base::AddContentsVisualOverflow(
          physical_fragment->ComputeContentsInkOverflow().ToLayoutFlippedRect(
              physical_fragment->Style(), physical_fragment->Size()));
      // TODO(kojii): The above code computes visual overflow only, we fallback
      // to LayoutBlock for AddLayoutOverflow() for now. It doesn't compute
      // correctly without RootInlineBox though.
    }
  }
  Base::AddVisualOverflowFromChildren();
}

template <typename Base>
void LayoutNGMixin<Base>::AddLayoutOverflowFromChildren() {
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
void LayoutNGMixin<Base>::AddScrollingOverflowFromChildren() {
  bool children_inline = Base::ChildrenInline();

  const NGPhysicalBoxFragment* physical_fragment = CurrentFragment();
  DCHECK(physical_fragment);
  // inline-end LayoutOverflow padding spec is still undecided:
  // https://github.com/w3c/csswg-drafts/issues/129
  // For backwards compatibility, if container clips overflow,
  // padding is added to the inline-end for inline children.
  base::Optional<NGPhysicalBoxStrut> padding_strut;
  if (Base::HasOverflowClip()) {
    padding_strut =
        NGBoxStrut(LayoutUnit(), Base::PaddingEnd(), LayoutUnit(), LayoutUnit())
            .ConvertToPhysical(Base::StyleRef().GetWritingMode(),
                               Base::StyleRef().Direction());
  }

  NGPhysicalOffsetRect children_overflow;

  // Only add overflow for fragments NG has not reflected into Legacy.
  // These fragments are:
  // - inline fragments,
  // - out of flow fragments whose css container is inline box.
  // TODO(layout-dev) Transfroms also need to be applied to compute overflow
  // correctly. NG is not yet transform-aware. crbug.com/855965
  if (!physical_fragment->Children().IsEmpty()) {
    for (const auto& child : physical_fragment->Children()) {
      NGPhysicalOffsetRect child_scrollable_overflow;
      if (child->IsOutOfFlowPositioned()) {
        child_scrollable_overflow = child->ScrollableOverflow();
      } else if (children_inline && child->IsLineBox()) {
        DCHECK(child->IsLineBox());
        child_scrollable_overflow =
            ToNGPhysicalLineBoxFragment(*child).ScrollableOverflow(
                Base::Style(), physical_fragment->Size());
        if (padding_strut)
          child_scrollable_overflow.Expand(*padding_strut);
      } else {
        continue;
      }
      child_scrollable_overflow.offset += child.Offset();
      children_overflow.Unite(child_scrollable_overflow);
    }
  }

  // LayoutOverflow takes flipped blocks coordinates, adjust as needed.
  LayoutRect children_flipped_overflow = children_overflow.ToLayoutFlippedRect(
      physical_fragment->Style(), physical_fragment->Size());
  Base::AddLayoutOverflow(children_flipped_overflow);
}

template <typename Base>
void LayoutNGMixin<Base>::AddOutlineRects(
    Vector<LayoutRect>& rects,
    const LayoutPoint& additional_offset,
    NGOutlineType include_block_overflows) const {
  if (PaintFragment()) {
    PaintFragment()->AddSelfOutlineRect(&rects, additional_offset,
                                        include_block_overflows);
  } else {
    Base::AddOutlineRects(rects, additional_offset, include_block_overflows);
  }
}

// Retrieve NGBaseline from the current fragment.
template <typename Base>
const NGBaseline* LayoutNGMixin<Base>::FragmentBaseline(
    NGBaselineAlgorithmType type) const {
  if (const NGPhysicalFragment* physical_fragment = CurrentFragment()) {
    FontBaseline baseline_type = Base::StyleRef().GetFontBaseline();
    return ToNGPhysicalBoxFragment(physical_fragment)
        ->Baseline({type, baseline_type});
  }
  return nullptr;
}

template <typename Base>
LayoutUnit LayoutNGMixin<Base>::FirstLineBoxBaseline() const {
  if (Base::ChildrenInline()) {
    if (const NGBaseline* baseline =
            FragmentBaseline(NGBaselineAlgorithmType::kFirstLine)) {
      return baseline->offset;
    }
  }
  return Base::FirstLineBoxBaseline();
}

template <typename Base>
LayoutUnit LayoutNGMixin<Base>::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  if (Base::ChildrenInline()) {
    if (const NGBaseline* baseline =
            FragmentBaseline(NGBaselineAlgorithmType::kAtomicInline)) {
      return baseline->offset;
    }
  }
  return Base::InlineBlockBaseline(line_direction);
}

template <typename Base>
scoped_refptr<NGLayoutResult> LayoutNGMixin<Base>::CachedLayoutResult(
    const NGConstraintSpace& constraint_space,
    const NGBreakToken* break_token) const {
  if (!RuntimeEnabledFeatures::LayoutNGFragmentCachingEnabled())
    return nullptr;
  if (!cached_result_ || !Base::cached_constraint_space_ || break_token ||
      Base::NeedsLayout())
    return nullptr;
  if (constraint_space != *Base::cached_constraint_space_)
    return nullptr;
  // The checks above should be enough to bail if layout is incomplete, but
  // let's verify:
  DCHECK(
      IsBlockLayoutComplete(*Base::cached_constraint_space_, *cached_result_));
  // If we used to contain abspos items, we can't reuse the fragment, because
  // we can't be sure that the list of items hasn't changed (as we bubble them
  // up during layout). In the case of newly-added abspos items to this
  // containing block, we will handle those by the NeedsLayout check above for
  // now.
  // TODO(layout-ng): Come up with a better solution for this
  if (cached_result_->OutOfFlowPositionedDescendants().size())
    return nullptr;
  return base::AdoptRef(new NGLayoutResult(*cached_result_));
}

template <typename Base>
void LayoutNGMixin<Base>::SetCachedLayoutResult(
    const NGConstraintSpace& constraint_space,
    const NGBreakToken* break_token,
    const NGLayoutResult& layout_result) {
  if (break_token || layout_result.Status() != NGLayoutResult::kSuccess) {
    // We can't cache these yet
    return;
  }
  if (constraint_space.IsIntermediateLayout())
    return;

  Base::cached_constraint_space_.reset(new NGConstraintSpace(constraint_space));
  cached_result_ = &layout_result;
}

template <typename Base>
void LayoutNGMixin<Base>::ClearCachedLayoutResult() {
  cached_result_.reset();
  Base::cached_constraint_space_.reset();
}

template <typename Base>
scoped_refptr<const NGLayoutResult>
LayoutNGMixin<Base>::CachedLayoutResultForTesting() {
  return cached_result_;
}

template <typename Base>
void LayoutNGMixin<Base>::SetPaintFragment(
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPhysicalOffset offset,
    scoped_refptr<NGPaintFragment>* current) {
  DCHECK(current);
  *current = fragment ? NGPaintFragment::Create(std::move(fragment), offset,
                                                std::move(*current))
                      : nullptr;
}

template <typename Base>
void LayoutNGMixin<Base>::SetPaintFragment(
    const NGBreakToken* break_token,
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPhysicalOffset offset) {
  // TODO(kojii): There are cases where the first call has break_token.
  // Investigate why and handle appropriately.
  // DCHECK(!break_token || paint_fragment_);
  scoped_refptr<NGPaintFragment>* current =
      NGPaintFragment::Find(&paint_fragment_, break_token);
  SetPaintFragment(std::move(fragment), offset, current);
}

template <typename Base>
void LayoutNGMixin<Base>::UpdatePaintFragmentFromCachedLayoutResult(
    const NGBreakToken* break_token,
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPhysicalOffset fragment_offset) {
  DCHECK(fragment);
  // TODO(kojii): There are cases where the first call has break_token.
  // Investigate why and handle appropriately.
  // DCHECK(!break_token || paint_fragment_);
  scoped_refptr<NGPaintFragment>* current =
      NGPaintFragment::Find(&paint_fragment_, break_token);
  DCHECK(current);
  DCHECK(*current);
  (*current)->UpdateFromCachedLayoutResult(std::move(fragment),
                                           fragment_offset);
}

template <typename Base>
void LayoutNGMixin<Base>::InvalidateDisplayItemClients(
    PaintInvalidationReason invalidation_reason) const {
  if (NGPaintFragment* fragment = PaintFragment()) {
    // TODO(koji): Should be in the PaintInvalidator, possibly with more logic
    // ported from BlockFlowPaintInvalidator.
    ObjectPaintInvalidator object_paint_invalidator(*this);
    object_paint_invalidator.InvalidateDisplayItemClient(*fragment,
                                                         invalidation_reason);
    return;
  }

  LayoutBlockFlow::InvalidateDisplayItemClients(invalidation_reason);
}

template <typename Base>
void LayoutNGMixin<Base>::Paint(const PaintInfo& paint_info) const {
  if (PaintFragment())
    NGBlockFlowPainter(*this).Paint(paint_info);
  else
    LayoutBlockFlow::Paint(paint_info);
}

template <typename Base>
bool LayoutNGMixin<Base>::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  if (!PaintFragment()) {
    return LayoutBlockFlow::NodeAtPoint(result, location_in_container,
                                        accumulated_offset, action);
  }
  // In LayoutBox::NodeAtPoint() and subclass overrides, it is guaranteed that
  // |accumulated_offset + Location()| equals the physical offset of the current
  // LayoutBox in the paint layer, regardless of writing mode or whether the box
  // was placed by NG or legacy.
  const LayoutPoint physical_offset = accumulated_offset + Base::Location();
  if (!this->IsEffectiveRootScroller()) {
    // Check if we need to do anything at all.
    // If we have clipping, then we can't have any spillout.
    LayoutRect overflow_box = Base::HasOverflowClip()
                                  ? Base::BorderBoxRect()
                                  : Base::VisualOverflowRect();
    overflow_box.MoveBy(physical_offset);
    if (!location_in_container.Intersects(overflow_box))
      return false;
  }
  if (Base::IsInSelfHitTestingPhase(action) && Base::HasOverflowClip() &&
      Base::HitTestOverflowControl(result, location_in_container,
                                   physical_offset))
    return true;

  return NGBlockFlowPainter(*this).NodeAtPoint(result, location_in_container,
                                               physical_offset, action);
}

template <typename Base>
PositionWithAffinity LayoutNGMixin<Base>::PositionForPoint(
    const LayoutPoint& point) const {
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

  const PositionWithAffinity ng_position =
      PaintFragment()->PositionForPoint(NGPhysicalOffset(point));
  if (ng_position.IsNotNull())
    return ng_position;
  return Base::CreatePositionWithAffinity(0);
}

template <typename Base>
void LayoutNGMixin<Base>::DirtyLinesFromChangedChild(
    LayoutObject* child,
    MarkingBehavior marking_behavior) {
  DCHECK_EQ(marking_behavior, kMarkContainerChain);
  NGPaintFragment::DirtyLinesFromChangedChild(child);
}

template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutTableCaption>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutTableCell>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlockFlow>;

}  // namespace blink
