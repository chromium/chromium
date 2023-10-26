// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node_data.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(ContainerNode* node)
    : LayoutBlockFlow(node) {}

LayoutNGBlockFlow::~LayoutNGBlockFlow() = default;

void LayoutNGBlockFlow::Trace(Visitor* visitor) const {
  visitor->Trace(ng_inline_node_data_);
  LayoutBlockFlow::Trace(visitor);
}

bool LayoutNGBlockFlow::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGBlockFlow || LayoutBlockFlow::IsOfType(type);
}

void LayoutNGBlockFlow::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlockFlow::StyleDidChange(diff, old_style);

  if (diff.NeedsReshape()) {
    SetNeedsCollectInlines();
  }
}

InlineNodeData* LayoutNGBlockFlow::TakeInlineNodeData() {
  NOT_DESTROYED();
  return ng_inline_node_data_.Release();
}

InlineNodeData* LayoutNGBlockFlow::GetInlineNodeData() const {
  NOT_DESTROYED();
  return ng_inline_node_data_.Get();
}

void LayoutNGBlockFlow::ResetInlineNodeData() {
  NOT_DESTROYED();
  ng_inline_node_data_ = MakeGarbageCollected<InlineNodeData>();
}

void LayoutNGBlockFlow::ClearInlineNodeData() {
  NOT_DESTROYED();
  if (ng_inline_node_data_) {
    // ng_inline_node_data_ is not used from now on but exists until GC happens,
    // so it is better to eagerly clear HeapVector to improve memory
    // utilization.
    ng_inline_node_data_->items.clear();
    ng_inline_node_data_.Clear();
  }
}

void LayoutNGBlockFlow::AddOutlineRects(
    OutlineRectCollector& collector,
    LayoutObject::OutlineInfo* info,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  NOT_DESTROYED();

  // TODO(crbug.com/1145048): Currently |NGBoxPhysicalFragment| does not support
  // NG block fragmentation. Fallback to the legacy code path.
  if (PhysicalFragmentCount() == 1) {
    const NGPhysicalBoxFragment* fragment = GetPhysicalFragment(0);
    if (fragment->HasItems()) {
      fragment->AddSelfOutlineRects(additional_offset, include_block_overflows,
                                    collector, info);
      return;
    }
  }

  LayoutBlockFlow::AddOutlineRects(collector, info, additional_offset,
                                   include_block_overflows);
}

bool LayoutNGBlockFlow::NodeAtPoint(HitTestResult& result,
                                    const HitTestLocation& hit_test_location,
                                    const PhysicalOffset& accumulated_offset,
                                    HitTestPhase phase) {
  NOT_DESTROYED();

  // Please see |LayoutBlock::Paint()| for these DCHECKs.
  DCHECK(IsMonolithic() || !CanTraversePhysicalFragments() ||
         !Parent()->CanTraversePhysicalFragments());
  // We may get here in multiple-fragment cases if the object is repeated
  // (inside table headers and footers, for instance).
  DCHECK(PhysicalFragmentCount() <= 1u ||
         GetPhysicalFragment(0)->BreakToken()->IsRepeated());

  if (!MayIntersect(result, hit_test_location, accumulated_offset)) {
    return false;
  }

  if (PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = GetPhysicalFragment(0);
    DCHECK(fragment);
    if (fragment->HasItems() ||
        // Check descendants of this fragment because floats may be in the
        // |FragmentItems| of the descendants.
        (phase == HitTestPhase::kFloat &&
         fragment->HasFloatingDescendantsForPaint())) {
      return NGBoxFragmentPainter(*fragment).NodeAtPoint(
          result, hit_test_location, accumulated_offset, phase);
    }
  }

  return LayoutBlockFlow::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, phase);
}

PositionWithAffinity LayoutNGBlockFlow::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  if (IsAtomicInlineLevel()) {
    const PositionWithAffinity atomic_inline_position =
        PositionForPointIfOutsideAtomicInlineLevel(point);
    if (atomic_inline_position.IsNotNull()) {
      return atomic_inline_position;
    }
  }

  if (!ChildrenInline()) {
    return LayoutBlock::PositionForPoint(point);
  }

  if (PhysicalFragmentCount()) {
    return PositionForPointInFragments(point);
  }

  return CreatePositionWithAffinity(0);
}

void LayoutNGBlockFlow::DirtyLinesFromChangedChild(LayoutObject* child) {
  NOT_DESTROYED();

  // We need to dirty line box fragments only if the child is once laid out in
  // LayoutNG inline formatting context. New objects are handled in
  // InlineNode::MarkLineBoxesDirty().
  if (child->IsInLayoutNGInlineFormattingContext()) {
    FragmentItems::DirtyLinesFromChangedChild(*child, *this);
  }
}

}  // namespace blink
