// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

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

  // https://drafts.csswg.org/css-align/#distribution-block
  // All values other than normal force the block container to establish an
  // independent formatting context.
  if (RuntimeEnabledFeatures::AlignContentForBlocksEnabled()) {
    if (StyleRef().AlignContent().GetPosition() != ContentPosition::kNormal ||
        StyleRef().AlignContent().Distribution() !=
            ContentDistributionType::kDefault) {
      return true;
    }
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
