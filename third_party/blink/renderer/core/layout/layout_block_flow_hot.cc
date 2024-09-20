// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

void LayoutBlockFlow::Trace(Visitor* visitor) const {
  visitor->Trace(multi_column_flow_thread_);
  visitor->Trace(inline_node_data_);
  LayoutBlock::Trace(visitor);
}

DISABLE_CFI_PERF
bool LayoutBlockFlow::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  if (IsInline() || IsFloatingOrOutOfFlowPositioned() || IsScrollContainer() ||
      IsFlexItem() || IsCustomItem() || IsDocumentElement() || IsGridItem() ||
      IsWritingModeRoot() || IsMathItem() ||
      StyleRef().Display() == EDisplay::kFlowRoot ||
      StyleRef().Display() == EDisplay::kFlowRootListItem ||
      ShouldApplyPaintContainment() || ShouldApplyLayoutContainment() ||
      StyleRef().HasLineClamp() || StyleRef().SpecifiesColumns() ||
      StyleRef().GetColumnSpan() == EColumnSpan::kAll) {
    // The specs require this object to establish a new formatting context.
    return true;
  }

  if (RuntimeEnabledFeatures::CanvasPlaceElementEnabled() &&
      Parent()->IsCanvas()) {
    return true;
  }

  if (RuntimeEnabledFeatures::ContainerTypeNoLayoutContainmentEnabled()) {
    if (StyleRef().IsContainerForSizeContainerQueries()) {
      return true;
    }
  }

  // https://drafts.csswg.org/css-align/#distribution-block
  // All values other than normal force the block container to establish an
  // independent formatting context.
  if (StyleRef().AlignContent().GetPosition() != ContentPosition::kNormal ||
      StyleRef().AlignContent().Distribution() !=
          ContentDistributionType::kDefault) {
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

  if (diff.NeedsReshape()) {
    SetNeedsCollectInlines();

    // The `initial-letter` creates a special `InlineItem`. When it's turned
    // on/off, its parent IFC should run `CollectInlines()`.
    const ComputedStyle& new_style = StyleRef();
    if (old_style->InitialLetter().IsNormal() !=
        new_style.InitialLetter().IsNormal()) [[unlikely]] {
      if (LayoutObject* parent = Parent()) {
        parent->SetNeedsCollectInlines();
      }
    }
  }
}

}  // namespace blink
