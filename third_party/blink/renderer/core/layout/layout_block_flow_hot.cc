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
  if (!EverHadLayout() && !ContainsFloats())
    return;

  if (descendants_with_floats_marked_for_layout_ && !float_to_remove)
    return;
  descendants_with_floats_marked_for_layout_ |= !float_to_remove;

  MarkingBehavior mark_parents =
      in_layout ? kMarkOnlyThis : kMarkContainerChain;
  SetChildNeedsLayout(mark_parents);

  if (float_to_remove)
    RemoveFloatingObject(float_to_remove);

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
      if ((float_to_remove ? child_block_flow->ContainsFloat(float_to_remove)
                           : child_block_flow->ContainsFloats()) ||
          child_block_flow->ShrinkToAvoidFloats())
        child_block_flow->MarkAllDescendantsWithFloatsForLayout(float_to_remove,
                                                                in_layout);
    }
  }
}

void LayoutBlockFlow::Trace(Visitor* visitor) const {
  visitor->Trace(line_boxes_);
  visitor->Trace(rare_data_);
  visitor->Trace(floating_objects_);
  LayoutBlock::Trace(visitor);
}

DISABLE_CFI_PERF
bool LayoutBlockFlow::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  if (IsInline() || IsFloatingOrOutOfFlowPositioned() || IsScrollContainer() ||
      IsFlexItemIncludingDeprecatedAndNG() || IsCustomItem() ||
      IsDocumentElement() || IsGridItemIncludingNG() || IsWritingModeRoot() ||
      IsMathItem() || StyleRef().Display() == EDisplay::kFlowRoot ||
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

bool LayoutBlockFlow::ContainsFloat(LayoutBox* layout_box) const {
  NOT_DESTROYED();
  return floating_objects_ &&
         floating_objects_->Set().Contains<FloatingObjectHashTranslator>(
             layout_box);
}

DISABLE_CFI_PERF
void LayoutBlockFlow::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  NOT_DESTROYED();
  bool had_self_painting_layer = HasSelfPaintingLayer();
  LayoutBlock::StyleDidChange(diff, old_style);

  // After our style changed, if we lose our ability to propagate floats into
  // next sibling blocks, then we need to find the top most parent containing
  // that overhanging float and then mark its descendants with floats for layout
  // and clear all floats from its next sibling blocks that exist in our
  // floating objects list. See crbug.com/56299 and crbug.com/62875.
  bool can_propagate_float_into_sibling =
      !IsFloatingOrOutOfFlowPositioned() && !CreatesNewFormattingContext();
  bool sibling_float_propagation_changed =
      diff.NeedsFullLayout() && can_propagate_float_into_sibling_ &&
      !can_propagate_float_into_sibling && HasOverhangingFloats();

  // When this object's self-painting layer status changed, we should update
  // FloatingObjects::shouldPaint() flags for descendant overhanging floats in
  // ancestors.
  bool needs_update_ancestor_float_object_should_paint_flags = false;
  if (HasSelfPaintingLayer() != had_self_painting_layer &&
      HasOverhangingFloats()) {
    SetNeedsLayout(layout_invalidation_reason::kStyleChange);
    if (had_self_painting_layer)
      MarkAllDescendantsWithFloatsForLayout();
    else
      needs_update_ancestor_float_object_should_paint_flags = true;
  }

  if (sibling_float_propagation_changed ||
      needs_update_ancestor_float_object_should_paint_flags) {
    LayoutBlockFlow* parent_block_flow = this;
    const FloatingObjectSet& floating_object_set = floating_objects_->Set();
    FloatingObjectSetIterator end = floating_object_set.end();

    for (LayoutObject* curr = Parent(); !IsA<LayoutView>(curr);
         curr = curr->Parent()) {
      auto* curr_block = DynamicTo<LayoutBlockFlow>(curr);
      if (curr_block) {
        if (curr_block->HasOverhangingFloats()) {
          for (FloatingObjectSetIterator it = floating_object_set.begin();
               it != end; ++it) {
            LayoutBox* layout_box = (*it)->GetLayoutObject();
            if (curr_block->HasOverhangingFloat(layout_box)) {
              parent_block_flow = curr_block;
              break;
            }
          }
        }
      }
    }

    parent_block_flow->MarkAllDescendantsWithFloatsForLayout();
    if (sibling_float_propagation_changed)
      parent_block_flow->MarkSiblingsWithFloatsForLayout();
  }

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
