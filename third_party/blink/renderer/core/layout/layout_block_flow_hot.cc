// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

void LayoutBlockFlow::Trace(Visitor* visitor) const {
  visitor->Trace(inline_node_data_);
  LayoutBlock::Trace(visitor);
}

DISABLE_CFI_PERF
bool LayoutBlockFlow::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  if (IsInline() || IsFloatingOrOutOfFlowPositioned() || IsScrollContainer() ||
      IsFlexItem() || IsCustomItem() || IsDocumentElement() || IsGridItem() ||
      IsGridLanesItem() || IsWritingModeRoot() || IsMathItem() ||
      StyleRef().Display() == EDisplay::kFlowRoot ||
      StyleRef().Display() == EDisplay::kFlowRootListItem ||
      ShouldApplyPaintContainment() || ShouldApplyLayoutContainment() ||
      StyleRef().IsContainerForSizeContainerQueries() ||
      StyleRef().HasLineClamp() || StyleRef().SpecifiesColumns() ||
      StyleRef().GetColumnSpan() == EColumnSpan::kAll) {
    // The specs require this object to establish a new formatting context.
    return true;
  }

  if (RuntimeEnabledFeatures::CanvasDrawElementEnabled() &&
      Parent()->IsCanvas()) {
    return true;
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
void LayoutBlockFlow::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style, style_change_context);

  if (diff.NeedsFullLayout() || !old_style) {
    UpdateForMulticol();
  }
  if (old_style) {
    // We either gained or lost ::column style, trigger relayout to determine,
    // if column pseudo-elements are needed.
    if (old_style->CanGeneratePseudoElement(kPseudoIdColumn) !=
        StyleRef().CanGeneratePseudoElement(kPseudoIdColumn)) {
      SetNeedsLayout(layout_invalidation_reason::kStyleChange);
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
