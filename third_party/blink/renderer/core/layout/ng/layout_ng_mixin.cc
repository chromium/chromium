// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

template <typename Base>
LayoutNGMixin<Base>::LayoutNGMixin(ContainerNode* node) : Base(node) {
  Base::CheckIsNotDestroyed();
  static_assert(
      std::is_base_of<LayoutBlock, Base>::value,
      "Base class of LayoutNGMixin must be LayoutBlock or derived class.");
}

template <typename Base>
LayoutNGMixin<Base>::~LayoutNGMixin() = default;

template <typename Base>
void LayoutNGMixin<Base>::Paint(const PaintInfo& paint_info) const {
  Base::CheckIsNotDestroyed();

  // When |this| is NG block fragmented, the painter should traverse fragments
  // instead of |LayoutObject|, because this function cannot handle block
  // fragmented objects. We can come here only when |this| cannot traverse
  // fragments, or the parent is legacy.
  DCHECK(Base::IsMonolithic() || !Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  // We may get here in multiple-fragment cases if the object is repeated
  // (inside table headers and footers, for instance).
  DCHECK(Base::PhysicalFragmentCount() <= 1u ||
         Base::GetPhysicalFragment(0)->BreakToken()->IsRepeated());

  // Avoid painting dirty objects because descendants maybe already destroyed.
  if (UNLIKELY(Base::NeedsLayout() &&
               !Base::ChildLayoutBlockedByDisplayLock())) {
    NOTREACHED();
    return;
  }

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    NGBoxFragmentPainter(*fragment).Paint(paint_info);
    return;
  }

  NOTREACHED();
  Base::Paint(paint_info);
}

template <typename Base>
bool LayoutNGMixin<Base>::NodeAtPoint(HitTestResult& result,
                                      const HitTestLocation& hit_test_location,
                                      const PhysicalOffset& accumulated_offset,
                                      HitTestPhase phase) {
  Base::CheckIsNotDestroyed();

  // See |Paint()|.
  DCHECK(Base::IsMonolithic() || !Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  // We may get here in multiple-fragment cases if the object is repeated
  // (inside table headers and footers, for instance).
  DCHECK(Base::PhysicalFragmentCount() <= 1u ||
         Base::GetPhysicalFragment(0)->BreakToken()->IsRepeated());

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    return NGBoxFragmentPainter(*fragment).NodeAtPoint(
        result, hit_test_location, accumulated_offset, phase);
  }

  return false;
}

template <typename Base>
RecalcLayoutOverflowResult LayoutNGMixin<Base>::RecalcLayoutOverflow() {
  Base::CheckIsNotDestroyed();
  DCHECK(!NGDisableSideEffectsScope::IsDisabled());
  return Base::RecalcLayoutOverflowNG();
}

template <typename Base>
void LayoutNGMixin<Base>::RecalcVisualOverflow() {
  Base::CheckIsNotDestroyed();
  if (Base::CanUseFragmentsForVisualOverflow()) {
    Base::RecalcFragmentsVisualOverflow();
    return;
  }
  Base::RecalcVisualOverflow();
}

template <typename Base>
bool LayoutNGMixin<Base>::IsLayoutNGObject() const {
  Base::CheckIsNotDestroyed();
  return true;
}

template <typename Base>
MinMaxSizes LayoutNGMixin<Base>::ComputeIntrinsicLogicalWidths() const {
  Base::CheckIsNotDestroyed();
  DCHECK(!Base::IsTableCell());

  NGBlockNode node(const_cast<LayoutNGMixin<Base>*>(this));
  CHECK(node.CanUseNewLayout());

  NGConstraintSpace space = ConstraintSpaceForMinMaxSizes();
  return node
      .ComputeMinMaxSizes(node.Style().GetWritingMode(),
                          MinMaxSizesType::kContent, space)
      .sizes;
}

template <typename Base>
NGConstraintSpace LayoutNGMixin<Base>::ConstraintSpaceForMinMaxSizes() const {
  Base::CheckIsNotDestroyed();
  DCHECK(!Base::IsTableCell());
  const ComputedStyle& style = Base::StyleRef();

  NGConstraintSpaceBuilder builder(style.GetWritingMode(),
                                   style.GetWritingDirection(),
                                   /* is_new_fc */ true);
  builder.SetAvailableSize(
      {Base::ContainingBlockLogicalWidthForContent(),
       LayoutBoxUtils::AvailableLogicalHeight(*this, Base::ContainingBlock())});

  return builder.ToConstraintSpace();
}

template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlockFlow>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutSVGBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutView>;

}  // namespace blink
