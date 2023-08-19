// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

template <typename Base>
LayoutNGBlockFlowMixin<Base>::LayoutNGBlockFlowMixin(ContainerNode* node)
    : LayoutNGMixin<Base>(node) {
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
  Base::CheckIsNotDestroyed();
  Base::StyleDidChange(diff, old_style);

  if (diff.NeedsReshape()) {
    Base::SetNeedsCollectInlines();
  }
}

template <typename Base>
NGInlineNodeData* LayoutNGBlockFlowMixin<Base>::TakeNGInlineNodeData() {
  Base::CheckIsNotDestroyed();
  return ng_inline_node_data_.Release();
}

template <typename Base>
NGInlineNodeData* LayoutNGBlockFlowMixin<Base>::GetNGInlineNodeData() const {
  Base::CheckIsNotDestroyed();
  return ng_inline_node_data_;
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::ResetNGInlineNodeData() {
  Base::CheckIsNotDestroyed();
  ng_inline_node_data_ = MakeGarbageCollected<NGInlineNodeData>();

  // The offset_mapping determines the PlainText() output of text nodes,
  // and depends non-locally on children inside the block flow. For example
  // whitespace collapsing may happen or not based on the presence of a sibling
  // inline object.
  if (AXObjectCache* cache = Base::GetDocument().ExistingAXObjectCache()) {
    cache->TextOffsetsChanged(this);
  }
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::ClearNGInlineNodeData() {
  Base::CheckIsNotDestroyed();
  if (ng_inline_node_data_) {
    // ng_inline_node_data_ is not used from now on but exists until GC happens,
    // so it is better to eagerly clear HeapVector to improve memory
    // utilization.
    ng_inline_node_data_->items.clear();
    ng_inline_node_data_.Clear();
  }
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::AddOutlineRects(
    OutlineRectCollector& collector,
    LayoutObject::OutlineInfo* info,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  Base::CheckIsNotDestroyed();

  // TODO(crbug.com/1145048): Currently |NGBoxPhysicalFragment| does not support
  // NG block fragmentation. Fallback to the legacy code path.
  if (Base::PhysicalFragmentCount() == 1) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    if (fragment->HasItems()) {
      fragment->AddSelfOutlineRects(additional_offset, include_block_overflows,
                                    collector, info);
      return;
    }
  }

  Base::AddOutlineRects(collector, info, additional_offset,
                        include_block_overflows);
}

template <typename Base>
bool LayoutNGBlockFlowMixin<Base>::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestPhase phase) {
  Base::CheckIsNotDestroyed();

  // Please see |LayoutNGMixin<Base>::Paint()| for these DCHECKs.
  DCHECK(Base::IsMonolithic() || !Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  // We may get here in multiple-fragment cases if the object is repeated
  // (inside table headers and footers, for instance).
  DCHECK(Base::PhysicalFragmentCount() <= 1u ||
         Base::GetPhysicalFragment(0)->BreakToken()->IsRepeated());

  if (!Base::MayIntersect(result, hit_test_location, accumulated_offset))
    return false;

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    if (fragment->HasItems() ||
        // Check descendants of this fragment because floats may be in the
        // |NGFragmentItems| of the descendants.
        (phase == HitTestPhase::kFloat &&
         fragment->HasFloatingDescendantsForPaint())) {
      return NGBoxFragmentPainter(*fragment).NodeAtPoint(
          result, hit_test_location, accumulated_offset, phase);
    }
  }

  return LayoutBlockFlow::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, phase);
}

template <typename Base>
PositionWithAffinity LayoutNGBlockFlowMixin<Base>::PositionForPoint(
    const PhysicalOffset& point) const {
  Base::CheckIsNotDestroyed();
  DCHECK_GE(Base::GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  if (Base::IsAtomicInlineLevel()) {
    const PositionWithAffinity atomic_inline_position =
        Base::PositionForPointIfOutsideAtomicInlineLevel(point);
    if (atomic_inline_position.IsNotNull())
      return atomic_inline_position;
  }

  if (!Base::ChildrenInline())
    return LayoutBlock::PositionForPoint(point);

  if (Base::PhysicalFragmentCount())
    return Base::PositionForPointInFragments(point);

  return Base::CreatePositionWithAffinity(0);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::DirtyLinesFromChangedChild(
    LayoutObject* child) {
  Base::CheckIsNotDestroyed();

  // We need to dirty line box fragments only if the child is once laid out in
  // LayoutNG inline formatting context. New objects are handled in
  // NGInlineNode::MarkLineBoxesDirty().
  if (child->IsInLayoutNGInlineFormattingContext())
    NGFragmentItems::DirtyLinesFromChangedChild(*child, *this);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::Trace(Visitor* visitor) const {
  visitor->Trace(ng_inline_node_data_);
  LayoutNGMixin<Base>::Trace(visitor);
}

template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutBlockFlow>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutSVGBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutView>;

}  // namespace blink
