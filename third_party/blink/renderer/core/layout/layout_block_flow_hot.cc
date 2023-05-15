// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

void LayoutBlockFlow::MarkAllDescendantsWithFloatsForLayout(
    LayoutBox* float_to_remove,
    bool in_layout) {
  NOT_DESTROYED();
  if (!EverHadLayout()) {
    return;
  }

  if (descendants_with_floats_marked_for_layout_ && !float_to_remove)
    return;
  descendants_with_floats_marked_for_layout_ |= !float_to_remove;

  MarkingBehavior mark_parents =
      in_layout ? kMarkOnlyThis : kMarkContainerChain;
  SetChildNeedsLayout(mark_parents);

  // Iterate over our children and mark them as needed. If our children are
  // inline, then the only boxes which could contain floats are atomic inlines
  // (e.g. inline-block, float etc.) and these create formatting contexts, so
  // can't pick up intruding floats from ancestors/siblings - making them safe
  // to skip.
  if (!ChildrenInline()) {
    for (LayoutObject* child = FirstChild(); child;
         child = child->NextSibling()) {
      if ((!float_to_remove && child->IsFloatingOrOutOfFlowPositioned()) ||
          !child->IsLayoutBlock())
        continue;
      auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
      if (!child_block_flow) {
        auto* child_block = To<LayoutBlock>(child);
        if (child_block->ShrinkToAvoidFloats() && child_block->EverHadLayout())
          child_block->SetChildNeedsLayout(mark_parents);
        continue;
      }
      if (child_block_flow->ShrinkToAvoidFloats()) {
        child_block_flow->MarkAllDescendantsWithFloatsForLayout(float_to_remove,
                                                                in_layout);
      }
    }
  }
}

void LayoutBlockFlow::Trace(Visitor* visitor) const {
  visitor->Trace(multi_column_flow_thread_);
  LayoutBlock::Trace(visitor);
}

DISABLE_CFI_PERF
bool LayoutBlockFlow::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  if (IsInline() || IsFloatingOrOutOfFlowPositioned() || IsScrollContainer() ||
      IsFlexItemIncludingNG() || IsCustomItem() || IsDocumentElement() ||
      IsGridItemIncludingNG() || IsWritingModeRoot() || IsMathItem() ||
      StyleRef().Display() == EDisplay::kFlowRoot ||
      StyleRef().Display() == EDisplay::kFlowRootListItem ||
      ShouldApplyPaintContainment() || ShouldApplyLayoutContainment() ||
      StyleRef().IsDeprecatedWebkitBoxWithVerticalLineClamp() ||
      StyleRef().SpecifiesColumns() ||
      StyleRef().GetColumnSpan() == EColumnSpan::kAll) {
    // The specs require this object to establish a new formatting context.
    return true;
  }

  if (IsRenderedLegend())
    return true;

  if (ShouldBeConsideredAsReplaced())
    return true;

  return false;
}

DISABLE_CFI_PERF
void LayoutBlockFlow::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style);

  if (diff.NeedsFullLayout() || !old_style)
    CreateOrDestroyMultiColumnFlowThreadIfNeeded(old_style);
  if (old_style) {
    if (LayoutMultiColumnFlowThread* flow_thread = MultiColumnFlowThread()) {
      if (!StyleRef().ColumnRuleEquivalent(*old_style)) {
        // Column rules are painted by anonymous column set children of the
        // multicol container. We need to notify them.
        flow_thread->ColumnRuleStyleDidChange();
      }
    }
  }
}

}  // namespace blink
