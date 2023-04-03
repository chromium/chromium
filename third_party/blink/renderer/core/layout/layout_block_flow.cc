/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/glyph_overflow.h"
#include "third_party/blink/renderer/core/layout/line/inline_iterator.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/line_width.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/block_flow_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsLayoutBlockFlow : public LayoutBlock {
  LineBoxList line_boxes;
  Member<void*> member;
};

ASSERT_SIZE(LayoutBlockFlow, SameSizeAsLayoutBlockFlow);

// Some features, such as floats, margin collapsing and fragmentation, require
// some knowledge about things that happened when laying out previous block
// child siblings. Only looking at the object currently being laid out isn't
// always enough.
class BlockChildrenLayoutInfo {
  STACK_ALLOCATED();

 public:
  BlockChildrenLayoutInfo(LayoutBlockFlow* block_flow,
                          LayoutUnit before_edge,
                          LayoutUnit after_edge)
      : previous_break_after_value_(EBreakBetween::kAuto),
        is_at_first_in_flow_child_(true) {}

  // Store multicol layout state before first layout of a block child. The child
  // may contain a column spanner. If we need to re-lay out the block child
  // because our initial logical top estimate was wrong, we need to roll back to
  // how things were before laying out the child.
  void StoreMultiColumnLayoutState(const LayoutFlowThread& flow_thread) {
    multi_column_layout_state_ = flow_thread.GetMultiColumnLayoutState();
  }
  void RollBackToInitialMultiColumnLayoutState(LayoutFlowThread& flow_thread) {
    flow_thread.RestoreMultiColumnLayoutState(multi_column_layout_state_);
  }

  LayoutUnit& PreviousFloatLogicalBottom() {
    return previous_float_logical_bottom_;
  }

  EBreakBetween PreviousBreakAfterValue() const {
    return previous_break_after_value_;
  }
  void SetPreviousBreakAfterValue(EBreakBetween value) {
    previous_break_after_value_ = value;
  }

  bool IsAtFirstInFlowChild() const { return is_at_first_in_flow_child_; }
  void ClearIsAtFirstInFlowChild() { is_at_first_in_flow_child_ = false; }

 private:
  MultiColumnLayoutState multi_column_layout_state_;
  LayoutUnit previous_float_logical_bottom_;
  EBreakBetween previous_break_after_value_;
  bool is_at_first_in_flow_child_;
};

LayoutBlockFlow::LayoutBlockFlow(ContainerNode* node) : LayoutBlock(node) {
  SetChildrenInline(true);
}

#if DCHECK_IS_ON()
LayoutBlockFlow::~LayoutBlockFlow() {
  line_boxes_.AssertIsEmpty();
}
#else
LayoutBlockFlow::~LayoutBlockFlow() = default;
#endif

LayoutBlockFlow* LayoutBlockFlow::CreateAnonymous(
    Document* document,
    scoped_refptr<const ComputedStyle> style,
    LegacyLayout legacy) {
  LayoutBlockFlow* layout_block_flow =
      LayoutObjectFactory::CreateBlockFlow(*document, *style, legacy);
  layout_block_flow->SetDocumentForAnonymous(document);
  layout_block_flow->SetStyle(style);
  return layout_block_flow;
}

LayoutObject* LayoutBlockFlow::LayoutSpecialExcludedChild(
    bool relayout_children,
    SubtreeLayoutScope& layout_scope) {
  NOT_DESTROYED();
  LayoutMultiColumnFlowThread* flow_thread = MultiColumnFlowThread();
  if (!flow_thread)
    return nullptr;
  SetLogicalTopForChild(*flow_thread, BorderBefore() + PaddingBefore());
  flow_thread->LayoutColumns(layout_scope);
  DetermineLogicalLeftPositionForChild(*flow_thread);
  return flow_thread;
}

bool LayoutBlockFlow::UpdateLogicalWidthAndColumnWidth() {
  NOT_DESTROYED();
  bool relayout_children = LayoutBlock::UpdateLogicalWidthAndColumnWidth();
  if (LayoutMultiColumnFlowThread* flow_thread = MultiColumnFlowThread()) {
    if (flow_thread->NeedsNewWidth())
      return true;
  }
  return relayout_children;
}

void LayoutBlockFlow::SetBreakAtLineToAvoidWidow(int line_to_break) {
  NOT_DESTROYED();
  DCHECK_GE(line_to_break, 0);
  EnsureRareData();
  DCHECK(!rare_data_->did_break_at_line_to_avoid_widow_);
  rare_data_->line_break_to_avoid_widow_ = line_to_break;
}

void LayoutBlockFlow::SetDidBreakAtLineToAvoidWidow() {
  NOT_DESTROYED();
  DCHECK(!ShouldBreakAtLineToAvoidWidow());

  // This function should be called only after a break was applied to avoid
  // widows so assert |m_rareData| exists.
  DCHECK(rare_data_);

  rare_data_->did_break_at_line_to_avoid_widow_ = true;
}

void LayoutBlockFlow::ClearDidBreakAtLineToAvoidWidow() {
  NOT_DESTROYED();
  if (!rare_data_)
    return;

  rare_data_->did_break_at_line_to_avoid_widow_ = false;
}

void LayoutBlockFlow::ClearShouldBreakAtLineToAvoidWidow() const {
  NOT_DESTROYED();
  DCHECK(ShouldBreakAtLineToAvoidWidow());
  if (!rare_data_)
    return;

  rare_data_->line_break_to_avoid_widow_ = -1;
}

bool LayoutBlockFlow::IsInitialLetterBox() const {
  return IsA<FirstLetterPseudoElement>(GetNode()) &&
         !StyleRef().InitialLetter().IsNormal();
}

// TODO(1229581): Remove this function.
bool LayoutBlockFlow::IsSelfCollapsingBlock() const {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());
  DCHECK(CreatesNewFormattingContext());
  return false;
}

bool LayoutBlockFlow::CheckIfIsSelfCollapsingBlock() const {
  NOT_DESTROYED();
  // We are not self-collapsing if we
  // (a) have a non-zero height according to layout (an optimization to avoid
  //     wasting time)
  // (b) have border/padding,
  // (c) have a min-height
  // (d) have specified that one of our margins can't collapse using a CSS
  //     extension
  // (e) establish a new block formatting context.

  // The early exit must be done before we check for clean layout.
  // We should be able to give a quick answer if the box is a relayout boundary.
  // Being a relayout boundary implies a block formatting context, and also
  // our internal layout shouldn't affect our container in any way.
  if (CreatesNewFormattingContext())
    return false;

  // Placeholder elements are not laid out until the dimensions of their parent
  // text control are known, so they don't get layout until their parent has had
  // layout - this is unique in the layout tree and means when we call
  // isSelfCollapsingBlock on them we find that they still need layout.
  auto* element = DynamicTo<Element>(GetNode());
  DCHECK(!NeedsLayout() ||
         (element && element->ShadowPseudoId() ==
                         shadow_element_names::kPseudoInputPlaceholder));

  if (LogicalHeight() > LayoutUnit() ||
      StyleRef().LogicalMinHeight().IsPositive())
    return false;

  const Length& logical_height_length = StyleRef().LogicalHeight();
  bool has_auto_height = logical_height_length.IsAuto();
  if (logical_height_length.IsPercentOrCalc() &&
      !GetDocument().InQuirksMode()) {
    has_auto_height = true;
    if (LayoutBlock* cb = ContainingBlock()) {
      if (!IsA<LayoutView>(cb) &&
          (cb->StyleRef().LogicalHeight().IsFixed() || cb->IsTableCell()))
        has_auto_height = false;
    }
  }

  // If the height is 0 or auto, then whether or not we are a self-collapsing
  // block depends on whether we have content that is all self-collapsing.
  // TODO(alancutter): Make this work correctly for calc lengths.
  if (has_auto_height || ((logical_height_length.IsFixed() ||
                           logical_height_length.IsPercentOrCalc()) &&
                          logical_height_length.IsZero())) {
    // Marker_container should be a self-collapsing block. Marker_container is a
    // zero height anonymous block and marker is its only child.
    if (logical_height_length.IsFixed() && logical_height_length.IsZero() &&
        IsAnonymous() && Parent() && Parent()->IsListItem()) {
      LayoutObject* first_child = FirstChild();
      if (first_child && first_child->IsListMarker() &&
          !first_child->NextSibling())
        return true;
    }

    // If the block has inline children, see if we generated any line boxes.
    // If we have any line boxes, then we can't be self-collapsing, since we
    // have content.
    if (ChildrenInline())
      return !FirstLineBox();

    // Whether or not we collapse is dependent on whether all our normal flow
    // children are also self-collapsing.
    for (LayoutBox* child = FirstChildBox(); child;
         child = child->NextSiblingBox()) {
      if (child->IsFloatingOrOutOfFlowPositioned() || child->IsColumnSpanAll())
        continue;
      if (!child->IsSelfCollapsingBlock())
        return false;
    }
    return true;
  }
  return false;
}

DISABLE_CFI_PERF
void LayoutBlockFlow::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();

  // TODO(1229581): Remove this logic.
  NOTREACHED_NORETURN();

  DCHECK(NeedsLayout());
  DCHECK(IsInlineBlockOrInlineTable() || !IsInline());

  ClearOffsetMappingIfNeeded();

  if (!relayout_children && SimplifiedLayout())
    return;

  SubtreeLayoutScope layout_scope(*this);

  LayoutUnit previous_height = LogicalHeight();
  LayoutUnit old_left = LogicalLeft();
  bool logical_width_changed = UpdateLogicalWidthAndColumnWidth();
  relayout_children |= logical_width_changed;

  TextAutosizer::LayoutScope text_autosizer_layout_scope(this, &layout_scope);

  bool pagination_state_changed = pagination_state_changed_;
  bool intrinsic_logical_widths_were_dirty = IntrinsicLogicalWidthsDirty();

  // Multiple passes might be required for column based layout.
  // The number of passes could be as high as the number of columns.
  LayoutMultiColumnFlowThread* flow_thread = MultiColumnFlowThread();
  do {
    LayoutState state(*this, logical_width_changed);
    if (pagination_state_changed_) {
      // We now need a deep layout to clean up struts after pagination, if we
      // just ceased to be paginated, or, if we just became paginated on the
      // other hand, we now need the deep layout, to insert pagination struts.
      pagination_state_changed_ = false;
      state.SetPaginationStateChanged();
    }

    LayoutChildren(relayout_children, layout_scope);

    if (!intrinsic_logical_widths_were_dirty && IntrinsicLogicalWidthsDirty()) {
      // The only thing that should dirty preferred widths at this point is the
      // addition of overflow:auto scrollbars in a descendant. To avoid a
      // potential infinite loop, run layout again with auto scrollbars frozen
      // in their current state.
      PaintLayerScrollableArea::FreezeScrollbarsScope freeze_scrollbars;
      relayout_children |= UpdateLogicalWidthAndColumnWidth();
      LayoutChildren(relayout_children, layout_scope);
    }

    if (flow_thread && !flow_thread->FinishLayout()) {
      SetChildNeedsLayout(kMarkOnlyThis);
      continue;
    }

    if (ShouldBreakAtLineToAvoidWidow()) {
      SetEverHadLayout();
      continue;
    }
    break;
  } while (true);

  LayoutState state(*this, logical_width_changed);
  if (pagination_state_changed) {
    // We still haven't laid out positioned descendants, and we need to perform
    // a deep layout on those too if pagination state changed.
    state.SetPaginationStateChanged();
  }

  // Remember the automatic logical height we got from laying out the children.
  LayoutUnit unconstrained_client_after_edge = ClientLogicalBottom();

  // Adjust logical height to satisfy whatever computed style requires.
  UpdateLogicalHeight();

  if (LogicalHeight() != previous_height || IsDocumentElement())
    relayout_children = true;

  PositionedLayoutBehavior behavior = kDefaultLayout;
  if (old_left != LogicalLeft())
    behavior = kForcedLayoutAfterContainingBlockMoved;
  LayoutPositionedObjects(relayout_children, behavior);

  // Add overflow from children.
  ComputeLayoutOverflow(unconstrained_client_after_edge, false);

  descendants_with_floats_marked_for_layout_ = false;

  UpdateAfterLayout();

  ClearNeedsLayout();
  is_self_collapsing_ = CheckIfIsSelfCollapsingBlock();
}

DISABLE_CFI_PERF
void LayoutBlockFlow::ResetLayout() {
  NOT_DESTROYED();
  DCHECK(!IsLayoutNGObject()) << this;
  if (!FirstChild() && !IsAnonymousBlock())
    SetChildrenInline(true);

  // Text truncation kicks in if overflow isn't visible and text-overflow isn't
  // 'clip'. If this is an anonymous block, we have to examine the parent.
  // FIXME: CSS3 says that descendants that are clipped must also know how to
  // truncate. This is insanely difficult to figure out in general (especially
  // in the middle of doing layout), so we only handle the simple case of an
  // anonymous block truncating when its parent is clipped.
  // Walk all the lines and delete our ellipsis line boxes if they exist.
  if (ChildrenInline() && ShouldTruncateOverflowingText())
    DeleteEllipsisLineBoxes();

  if (View()->GetLayoutState()->IsPaginated()) {
    SetPaginationStrutPropagatedFromChild(LayoutUnit());
    SetFirstForcedBreakOffset(LayoutUnit());

    // Start with any applicable computed break-after and break-before values
    // for this object. During child layout, breakBefore will be joined with the
    // breakBefore value of the first in-flow child, and breakAfter will be
    // joined with the breakAfter value of the last in-flow child. This is done
    // in order to honor the requirement that a class A break point [1] may only
    // exists *between* in-flow siblings (i.e. not before the first child and
    // not after the last child).
    //
    // [1] https://drafts.csswg.org/css-break/#possible-breaks
    SetBreakBefore(LayoutBlock::BreakBefore());
    SetBreakAfter(LayoutBlock::BreakAfter());
  }
}

DISABLE_CFI_PERF
void LayoutBlockFlow::LayoutChildren(bool relayout_children,
                                     SubtreeLayoutScope& layout_scope) {
  NOT_DESTROYED();
  ResetLayout();

  if (ChildLayoutBlockedByDisplayLock())
    return;

  LayoutUnit before_edge = BorderBefore() + PaddingBefore();
  LayoutUnit after_edge = BorderAfter() + PaddingAfter();

  NGBoxStrut scrollbars = ComputeLogicalScrollbars();
  before_edge += scrollbars.block_start;
  after_edge += scrollbars.block_end;

  SetLogicalHeight(before_edge);

  if (ChildrenInline())
    LayoutInlineChildren(relayout_children, after_edge);
  else
    LayoutBlockChildren(relayout_children, layout_scope, before_edge,
                        after_edge);

  NotifyDisplayLockDidLayoutChildren();
}

DISABLE_CFI_PERF
void LayoutBlockFlow::DetermineLogicalLeftPositionForChild(LayoutBox& child) {
  NOT_DESTROYED();
  LayoutUnit start_position = BorderStart() + PaddingStart();
  LayoutUnit total_available_logical_width =
      BorderAndPaddingLogicalWidth() + AvailableLogicalWidth();

  if (ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    start_position -= LogicalLeftScrollbarWidth();
  else
    start_position += LogicalLeftScrollbarWidth();

  LayoutUnit child_margin_start = MarginStartForChild(child);
  LayoutUnit new_position = start_position + child_margin_start;

  SetLogicalLeftForChild(child, StyleRef().IsLeftToRightDirection()
                                    ? new_position
                                    : total_available_logical_width -
                                          new_position -
                                          LogicalWidthForChild(child));
}

void LayoutBlockFlow::SetLogicalLeftForChild(LayoutBox& child,
                                             LayoutUnit logical_left) {
  NOT_DESTROYED();
  LayoutPoint new_location(child.Location());
  if (IsHorizontalWritingMode()) {
    new_location.SetX(logical_left);
  } else {
    new_location.SetY(logical_left);
  }
  child.SetLocationAndUpdateOverflowControlsIfNeeded(new_location);
}

void LayoutBlockFlow::SetLogicalTopForChild(LayoutBox& child,
                                            LayoutUnit logical_top) {
  NOT_DESTROYED();
  if (IsHorizontalWritingMode()) {
    child.SetY(logical_top);
  } else {
    child.SetX(logical_top);
  }
}

bool LayoutBlockFlow::PositionAndLayoutOnceIfNeeded(
    LayoutBox& child,
    LayoutUnit new_logical_top,
    BlockChildrenLayoutInfo& layout_info) {
  NOT_DESTROYED();
  if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock())
    layout_info.RollBackToInitialMultiColumnLayoutState(*flow_thread);

  LayoutUnit& previous_float_logical_bottom =
      layout_info.PreviousFloatLogicalBottom();
  LayoutUnit lowest_float =
      std::max(previous_float_logical_bottom, LayoutUnit());

  LayoutUnit old_logical_top = LogicalTopForChild(child);
  SetLogicalTopForChild(child, new_logical_top);

  SubtreeLayoutScope layout_scope(child);
  auto child_needs_layout = [&child] {
    if (!child.NeedsLayout())
      return false;
    return child.SelfNeedsLayout() || !child.ChildLayoutBlockedByDisplayLock();
  };

  if (!child_needs_layout()) {
    // Like in MarkDescendantsWithFloatsForLayoutIfNeeded, we only need
    // to mark this object for layout if it actually is affected by a float
    if (new_logical_top != old_logical_top && child.ShrinkToAvoidFloats() &&
        (new_logical_top < lowest_float || old_logical_top < lowest_float)) {
      // The child's width is affected by adjacent floats. When the child shifts
      // to clear an item, its width can change (because it has more available
      // width).
      layout_scope.SetChildNeedsLayout(&child);
    } else {
      MarkChildForPaginationRelayoutIfNeeded(child, layout_scope);
    }
  }

  bool needed_layout = child_needs_layout();
  if (needed_layout)
    child.UpdateLayout();
  if (View()->GetLayoutState()->IsPaginated())
    UpdateFragmentationInfoForChild(child);
  return needed_layout;
}

void LayoutBlockFlow::InsertForcedBreakBeforeChildIfNeeded(
    LayoutBox& child,
    BlockChildrenLayoutInfo& layout_info) {
  NOT_DESTROYED();
  if (layout_info.IsAtFirstInFlowChild()) {
    // There's no class A break point before the first child (only *between*
    // siblings), so steal its break value and join it with what we already have
    // here.
    SetBreakBefore(
        JoinFragmentainerBreakValues(BreakBefore(), child.BreakBefore()));

    return;
  }

  // Figure out if a forced break should be inserted in front of the child. If
  // we insert a forced break, the margins on this child may not collapse with
  // those preceding the break.
  EBreakBetween class_a_break_point_value =
      child.ClassABreakPointValue(layout_info.PreviousBreakAfterValue());

  if (IsForcedFragmentainerBreakValue(class_a_break_point_value)) {
    LayoutUnit old_logical_top = LogicalHeight();
    LayoutUnit new_logical_top =
        ApplyForcedBreak(old_logical_top, class_a_break_point_value);
    SetLogicalHeight(new_logical_top);
    LayoutUnit pagination_strut = new_logical_top - old_logical_top;
    child.SetPaginationStrut(pagination_strut);
  }
}

void LayoutBlockFlow::LayoutBlockChild(LayoutBox& child,
                                       BlockChildrenLayoutInfo& layout_info) {
  NOT_DESTROYED();
  auto* child_layout_block_flow = DynamicTo<LayoutBlockFlow>(&child);

  // The child is a normal flow object. Compute the margins we will use for
  // collapsing now.
  child.ComputeAndSetBlockDirectionMargins(this);

  // Try to guess our correct logical top position. In most cases this guess
  // will be correct. Only if we're wrong (when we compute the real logical top
  // position) will we have to potentially relayout.
  LayoutUnit estimate_without_pagination;
  LayoutUnit logical_top_estimate = LogicalHeight();

  if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock())
    layout_info.StoreMultiColumnLayoutState(*flow_thread);

  // Use the estimated block position and lay out the child if needed. After
  // child layout, when we have enough information to perform proper margin
  // collapsing, float clearing and pagination, we may have to reposition and
  // lay out again if the estimate was wrong.
  PositionAndLayoutOnceIfNeeded(child, logical_top_estimate, layout_info);

  // Cache if we are at the top of the block right now.
  bool paginated = View()->GetLayoutState()->IsPaginated();

  // If there should be a forced break before the child, we need to insert it
  // before attempting to collapse margins or apply clearance.
  if (paginated) {
    // We will now insert the strut needed by any forced break. After this
    // operation, we will have calculated the offset where we can apply margin
    // collapsing and clearance. After having applied those things, we'll be at
    // the position where we can honor requirements of unbreakable content,
    // which may extend the strut further.
    child.ResetPaginationStrut();
    InsertForcedBreakBeforeChildIfNeeded(child, layout_info);
  }

  // Now determine the correct ypos based off examination of collapsing margin
  // values.
  LayoutUnit new_logical_top = LogicalHeight();

  // If there's a forced break in front of this child, its final position has
  // already been determined. Otherwise, see if there are other reasons for
  // breaking before it (break-inside:avoid, or not enough space for the first
  // piece of child content to fit in the current fragmentainer), and adjust the
  // position accordingly.
  if (paginated) {
    if (estimate_without_pagination != new_logical_top) {
      // We got a new position due to clearance or margin collapsing. Before we
      // attempt to paginate (which may result in the position changing again),
      // let's try again at the new position (since a new position may result in
      // a new logical height).
      PositionAndLayoutOnceIfNeeded(child, new_logical_top, layout_info);
    }

    // We have now applied forced breaks, margin collapsing and clearance, and
    // we're at the position where we can honor requirements of unbreakable
    // content.
    new_logical_top = AdjustBlockChildForPagination(new_logical_top, child,
                                                    layout_info, false);
  }

  // Clearance, margin collapsing or pagination may have given us a new logical
  // top, in which case we may have to reposition and possibly relayout as well.
  // If we determined during child layout that we need to insert a break to
  // honor widows, we also need to relayout.
  if (new_logical_top != logical_top_estimate || child.NeedsLayout() ||
      (paginated && child_layout_block_flow &&
       child_layout_block_flow->ShouldBreakAtLineToAvoidWidow())) {
    PositionAndLayoutOnceIfNeeded(child, new_logical_top, layout_info);
  }

  // Now place the child in the correct left position
  DetermineLogicalLeftPositionForChild(child);

  // Update our height now that the child has been placed in the correct
  // position.
  SetLogicalHeight(LogicalHeight() + LogicalHeightForChild(child));

  if (paginated) {
    // Keep track of the break-after value of the child, so that it can be
    // joined with the break-before value of the next in-flow object at the next
    // class A break point.
    layout_info.SetPreviousBreakAfterValue(child.BreakAfter());

    PaginatedContentWasLaidOut(child.LogicalBottom());

    if (child_layout_block_flow) {
      // If a forced break was inserted inside the child, translate and
      // propagate the offset to this object.
      if (LayoutUnit offset = child_layout_block_flow->FirstForcedBreakOffset())
        SetFirstForcedBreakOffset(offset + new_logical_top);
    }
  }

  if (child.IsLayoutMultiColumnSpannerPlaceholder()) {
    // The actual column-span:all element is positioned by this placeholder
    // child.
    PositionSpannerDescendant(To<LayoutMultiColumnSpannerPlaceholder>(child));
  }
}

LayoutUnit LayoutBlockFlow::AdjustBlockChildForPagination(
    LayoutUnit logical_top,
    LayoutBox& child,
    BlockChildrenLayoutInfo& layout_info,
    bool at_before_side_of_block) {
  NOT_DESTROYED();
  auto* child_block_flow = DynamicTo<LayoutBlockFlow>(&child);

  // See if we need a soft (unforced) break in front of this child, and set the
  // pagination strut in that case. An unforced break may come from two sources:
  // 1. The first piece of content inside the child doesn't fit in the current
  //    page or column
  // 2. The child itself has breaking restrictions (break-inside:avoid, replaced
  //    content, etc.) and doesn't fully fit in the current page or column.
  //
  // No matter which source, if we need to insert a strut, it should always take
  // us to the exact top of a page or column further ahead, or be zero.

  // The first piece of content inside the child may have set a strut during
  // layout. Currently, only block flows support strut propagation, but this may
  // (and should) change in the future. See crbug.com/539873
  LayoutUnit strut_from_content =
      child_block_flow ? child_block_flow->PaginationStrutPropagatedFromChild()
                       : LayoutUnit();
  LayoutUnit logical_top_with_content_strut = logical_top + strut_from_content;

  LayoutUnit logical_top_after_unsplittable =
      AdjustForUnsplittableChild(child, logical_top);

  // Pick the largest offset. Tall unsplittable content may take us to a page or
  // column further ahead than the next one.
  LayoutUnit logical_top_after_pagination =
      std::max(logical_top_with_content_strut, logical_top_after_unsplittable);
  LayoutUnit new_logical_top = logical_top;

  // Forced breaks may already have caused a strut, and this needs to be added
  // together with any strut detected here in this method.
  LayoutUnit previous_strut = child.PaginationStrut();

  if (LayoutUnit pagination_strut =
          logical_top_after_pagination - logical_top + previous_strut) {
    DCHECK_GT(pagination_strut, 0);
    // If we're not at the first in-flow child, there's a class A break point
    // before the child. If we *are* at the first in-flow child, but the child
    // isn't flush with the content edge of its container, due to e.g.
    // clearance, there's a class C break point before the child. Otherwise we
    // should propagate the strut to our parent block, and attempt to break
    // there instead. See https://drafts.csswg.org/css-break/#possible-breaks
    bool can_break =
        !layout_info.IsAtFirstInFlowChild() || !at_before_side_of_block;
    if (!can_break && child.IsMonolithic() && !AllowsPaginationStrut()) {
      // The child is monolithic content, e.g. an image. It is truly
      // unsplittable. Breaking inside it would be bad. Since this block doesn't
      // allow pagination struts to be propagated to it, we're left to handle it
      // on our own right here. Break before the child, even if we're currently
      // at the block start (i.e. there's no class A or C break point here).
      can_break = true;
    }
    if (can_break) {
      child.SetPaginationStrut(pagination_strut);
      // |previousStrut| was already baked into the logical top, so don't add
      // it again.
      new_logical_top += pagination_strut - previous_strut;
    } else {
      // No valid break point here. Propagate the strut from the child to this
      // block, but only if the block allows it. If the block doesn't allow it,
      // we'll just ignore the strut and carry on, without breaking. This
      // happens e.g. when a tall break-inside:avoid object with a top margin is
      // the first in-flow child in the fragmentation context.
      if (AllowsPaginationStrut()) {
        pagination_strut += logical_top;
        SetPaginationStrutPropagatedFromChild(pagination_strut);
        if (child_block_flow)
          child_block_flow->SetPaginationStrutPropagatedFromChild(LayoutUnit());
      }
      child.ResetPaginationStrut();
    }
  }

  // Similar to how we apply clearance. Go ahead and boost height() to be the
  // place where we're going to position the child.
  SetLogicalHeight(LogicalHeight() + (new_logical_top - logical_top));

  // Return the final adjusted logical top.
  return new_logical_top;
}

LayoutUnit LayoutBlockFlow::AdjustFloatLogicalTopForPagination(
    LayoutBox& child,
    LayoutUnit logical_top_margin_edge) {
  NOT_DESTROYED();
  // The first piece of content inside the child may have set a strut during
  // layout.
  LayoutUnit strut;
  auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
  if (child_block_flow)
    strut = child_block_flow->PaginationStrutPropagatedFromChild();

  LayoutUnit margin_before = MarginBeforeForChild(child);
  if (margin_before > LayoutUnit()) {
    // Avoid breaking inside the top margin of a float.
    if (strut) {
      // If we already had decided to break, just add the margin. The strut so
      // far only accounts for pushing the top border edge to the next
      // fragmentainer. We need to push the margin over as well, because
      // there's no break opportunity between margin and border.
      strut += margin_before;
    } else {
      // Even if we didn't break before the border box to the next
      // fragmentainer, we need to check if we can fit the margin before it.
      if (IsPageLogicalHeightKnown()) {
        LayoutUnit remaining_space = PageRemainingLogicalHeightForOffset(
            logical_top_margin_edge, kAssociateWithLatterPage);
        if (remaining_space <= margin_before) {
          strut += CalculatePaginationStrutToFitContent(logical_top_margin_edge,
                                                        margin_before);
        }
      }
    }
  }
  if (!strut) {
    // If we are unsplittable and don't fit, move to the next page or column
    // if that helps the situation.
    LayoutUnit new_logical_top_margin_edge =
        AdjustForUnsplittableChild(child, logical_top_margin_edge);
    strut = new_logical_top_margin_edge - logical_top_margin_edge;
  }

  child.SetPaginationStrut(strut);
  return logical_top_margin_edge + strut;
}

static bool ShouldSetStrutOnBlock(const LayoutBlockFlow& block,
                                  const RootInlineBox& line_box,
                                  LayoutUnit line_logical_offset,
                                  int line_index,
                                  LayoutUnit page_logical_height) {
  if (line_box == block.FirstRootBox()) {
    // This is the first line in the block. We can take the whole block with us
    // to the next page or column, rather than keeping a content-less portion of
    // it in the previous one. Only do this if the line is flush with the
    // content edge of the block, though. If it isn't, it means that the line
    // was pushed downwards by preceding floats that didn't fit beside the line,
    // and we don't want to move all that, since it has already been established
    // that it fits nicely where it is. In this case we have a class "C" break
    // point [1] in front of this line.
    //
    // [1] https://drafts.csswg.org/css-break/#possible-breaks
    if (line_logical_offset > block.BorderAndPaddingBefore())
      return false;

    LayoutUnit line_height =
        line_box.LineBottomWithLeading() - line_box.LineTopWithLeading();
    LayoutUnit total_logical_height =
        line_height + line_logical_offset.ClampNegativeToZero();
    // It's rather pointless to break before the block if the current line isn't
    // going to fit in the same column or page, so check that as well.
    if (total_logical_height > page_logical_height)
      return false;
  } else {
    if (line_index > block.StyleRef().Orphans())
      return false;

    // Not enough orphans here. Push the entire block to the next column / page
    // as an attempt to better satisfy the orphans requirement.
    //
    // Note that we should ideally check if the first line in the block is flush
    // with the content edge of the block here, because if it isn't, we should
    // break at the class "C" break point in front of the first line, rather
    // than before the entire block.
  }
  return block.AllowsPaginationStrut();
}

void LayoutBlockFlow::AdjustLinePositionForPagination(RootInlineBox& line_box,
                                                      LayoutUnit& delta) {
  NOT_DESTROYED();
  LayoutUnit logical_offset = line_box.LineTopWithLeading();
  LayoutUnit line_height = line_box.LineBottomWithLeading() - logical_offset;
  logical_offset += delta;
  line_box.SetPaginationStrut(LayoutUnit());
  line_box.SetIsFirstAfterPageBreak(false);
  LayoutState* layout_state = View()->GetLayoutState();
  if (!layout_state->IsPaginated())
    return;
  if (!IsPageLogicalHeightKnown())
    return;
  // Ruby annotations do not affect the size of the line box. Instead,
  // before-annotations are placed above the line's logical top, and
  // after-annotations are placed below the line's logical bottom. We need to
  // take this into consideration when block-fragmenting, so that we don't allow
  // breaking in the middle of an annotation.
  LayoutUnit logical_offset_with_annotations = logical_offset;
  LayoutUnit line_height_with_annotations = line_height;
  if (line_box.HasAnnotationsBefore()) {
    LayoutUnit adjustment =
        line_box.ComputeOverAnnotationAdjustment(logical_offset);
    logical_offset_with_annotations -= adjustment;
    line_height_with_annotations += adjustment;
  }
  if (line_box.HasAnnotationsAfter()) {
    line_height_with_annotations +=
        line_box.ComputeUnderAnnotationAdjustment(logical_offset + line_height);
  }
  LayoutUnit page_logical_height =
      PageLogicalHeightForOffset(logical_offset_with_annotations);
  LayoutUnit remaining_logical_height = PageRemainingLogicalHeightForOffset(
      logical_offset_with_annotations, kAssociateWithLatterPage);
  int line_index = LineCount(&line_box);
  if (remaining_logical_height < line_height_with_annotations ||
      (ShouldBreakAtLineToAvoidWidow() &&
       LineBreakToAvoidWidow() == line_index)) {
    LayoutUnit pagination_strut = CalculatePaginationStrutToFitContent(
        logical_offset_with_annotations, line_height_with_annotations);
    LayoutUnit new_logical_offset = logical_offset + pagination_strut;
    // Moving to a different page or column may mean that its height is
    // different.
    page_logical_height = PageLogicalHeightForOffset(new_logical_offset);
    // We need to insert a break now, either because there's no room for the
    // line in the current column / page, or because we have determined that we
    // need a break to satisfy widow requirements.
    if (ShouldBreakAtLineToAvoidWidow() &&
        LineBreakToAvoidWidow() == line_index) {
      ClearShouldBreakAtLineToAvoidWidow();
      SetDidBreakAtLineToAvoidWidow();
    } else if (line_height > page_logical_height) {
      // Too tall to fit in one page / column. Give up. Don't push to the next
      // page / column.
      PaginatedContentWasLaidOut(logical_offset + line_height);
      return;
    }

    if (ShouldSetStrutOnBlock(*this, line_box, logical_offset, line_index,
                              page_logical_height)) {
      // Note that when setting the strut on a block, it may be propagated to
      // parent blocks later on, if a block's logical top is flush with that of
      // its parent. We don't want content-less portions (struts) at the
      // beginning of a block before a break, if it can be avoided. After all,
      // that's the reason for setting struts on blocks and not lines in the
      // first place.
      SetPaginationStrutPropagatedFromChild(pagination_strut + logical_offset);
    } else {
      delta += pagination_strut;
      line_box.SetPaginationStrut(pagination_strut);
      line_box.SetIsFirstAfterPageBreak(true);
    }
    PaginatedContentWasLaidOut(new_logical_offset + line_height);
    return;
  }

  LayoutUnit strut_to_propagate;
  if (remaining_logical_height == page_logical_height) {
    // We're at the very top of a page or column.
    if (line_box != FirstRootBox())
      line_box.SetIsFirstAfterPageBreak(true);
    // If this is the first line in the block, and the block has a top border or
    // padding, we may want to set a strut on the block, so that everything ends
    // up in the next column or page. Setting a strut on the block is also
    // important when it comes to satisfying orphan requirements.
    if (ShouldSetStrutOnBlock(*this, line_box, logical_offset, line_index,
                              page_logical_height)) {
      DCHECK(!IsTableCell());
      strut_to_propagate =
          logical_offset + layout_state->HeightOffsetForTableHeaders();
    } else if (LayoutUnit pagination_strut =
                   layout_state->HeightOffsetForTableHeaders()) {
      delta += pagination_strut;
      line_box.SetPaginationStrut(pagination_strut);
    }
  } else if (line_box == FirstRootBox() && AllowsPaginationStrut()) {
    // This is the first line in the block. The block may still start in the
    // previous column or page, and if that's the case, attempt to pull it over
    // to where this line is, so that we don't split the top border or padding.
    LayoutUnit strut = remaining_logical_height + logical_offset +
                       layout_state->HeightOffsetForTableHeaders() -
                       page_logical_height;
    if (strut > LayoutUnit()) {
      // The block starts in a previous column or page. Set a strut on the block
      // if there's room for the top border, padding and the line in one column
      // or page.
      if (logical_offset + line_height <= page_logical_height)
        strut_to_propagate = strut;
    }
  }

  // If we found that some preceding content (lines, border and padding) belongs
  // together with this line, we should pull the entire block with us to the
  // fragmentainer we're currently in. We need to avoid this when the block
  // precedes the first fragmentainer, though. We shouldn't fragment content
  // there, but rather let it appear in the overflow area before the first
  // fragmentainer.
  if (strut_to_propagate && OffsetFromLogicalTopOfFirstPage() > LayoutUnit())
    SetPaginationStrutPropagatedFromChild(strut_to_propagate);

  PaginatedContentWasLaidOut(logical_offset + line_height);
}

LayoutUnit LayoutBlockFlow::AdjustForUnsplittableChild(
    LayoutBox& child,
    LayoutUnit logical_offset) const {
  NOT_DESTROYED();
  if (!child.IsMonolithic()) {
    return logical_offset;
  }
  LayoutUnit child_logical_height = LogicalHeightForChild(child);
  // Floats' margins do not collapse with page or column boundaries.
  if (child.IsFloating())
    child_logical_height +=
        MarginBeforeForChild(child) + MarginAfterForChild(child);
  if (!IsPageLogicalHeightKnown())
    return logical_offset;
  LayoutUnit remaining_logical_height = PageRemainingLogicalHeightForOffset(
      logical_offset, kAssociateWithLatterPage);
  if (remaining_logical_height >= child_logical_height)
    return logical_offset;  // It fits fine where it is. No need to break.
  LayoutUnit pagination_strut = CalculatePaginationStrutToFitContent(
      logical_offset, child_logical_height);
  if (pagination_strut == remaining_logical_height &&
      remaining_logical_height == PageLogicalHeightForOffset(logical_offset)) {
    // Don't break if we were at the top of a page, and we failed to fit the
    // content completely. No point in leaving a page completely blank.
    return logical_offset;
  }

  const auto* block_child = DynamicTo<LayoutBlockFlow>(child);
  if (block_child) {
    // If there's a forced break inside this object, figure out if we can fit
    // everything before that forced break in the current fragmentainer. If it
    // fits, we don't need to insert a break before the child.
    if (LayoutUnit first_break_offset = block_child->FirstForcedBreakOffset()) {
      if (remaining_logical_height >= first_break_offset)
        return logical_offset;
    }
  }

  return logical_offset + pagination_strut;
}

void LayoutBlockFlow::LayoutBlockChildren(bool relayout_children,
                                          SubtreeLayoutScope& layout_scope,
                                          LayoutUnit before_edge,
                                          LayoutUnit after_edge) {
  NOT_DESTROYED();
  DirtyForLayoutFromPercentageHeightDescendants(layout_scope);

  BlockChildrenLayoutInfo layout_info(this, before_edge, after_edge);

  // Fieldsets need to find their legend and position it inside the border of
  // the object.
  // The legend then gets skipped during normal layout. The same is true for
  // ruby text.
  // It doesn't get included in the normal layout process but is instead skipped
  LayoutObject* child_to_exclude =
      LayoutSpecialExcludedChild(relayout_children, layout_scope);

  for (auto* child = FirstChild(); child; child = child->NextSibling()) {
    child->SetShouldCheckForPaintInvalidation();

    if (child_to_exclude == child)
      continue;  // Skip this child, since it will be positioned by the
                 // specialized subclass (fieldsets and ruby runs).

    LayoutBox* box = To<LayoutBox>(child);
    UpdateBlockChildDirtyBitsBeforeLayout(relayout_children, *box);

    if (box->IsOutOfFlowPositioned()) {
      box->ContainingBlock()->InsertPositionedObject(box);
      AdjustPositionedBlock(*box, layout_info);
      continue;
    }
    if (box->IsFloating()) {
      NOTREACHED();
      continue;
    }
    if (box->IsColumnSpanAll()) {
      box->SpannerPlaceholder()->FlowThread()->SkipColumnSpanner(
          box, OffsetFromLogicalTopOfFirstPage() + LogicalHeight());
      continue;
    }

    // Lay out the child.
    LayoutBlockChild(*box, layout_info);
    layout_info.ClearIsAtFirstInFlowChild();
  }
}

void LayoutBlockFlow::AdjustPositionedBlock(
    LayoutBox& child,
    const BlockChildrenLayoutInfo& layout_info) {
  NOT_DESTROYED();
  LayoutUnit logical_top = LogicalHeight();

  // Forced breaks are only specified on in-flow objects, but auto-positioned
  // out-of-flow objects may be affected by a break-after value of the previous
  // in-flow object.
  if (View()->GetLayoutState()->IsPaginated())
    logical_top =
        ApplyForcedBreak(logical_top, layout_info.PreviousBreakAfterValue());

  UpdateStaticInlinePositionForChild(child, logical_top);

  PaintLayer* child_layer = child.Layer();
  if (child_layer->StaticBlockPosition() != logical_top)
    child_layer->SetStaticBlockPosition(logical_top);
}

LayoutUnit LayoutBlockFlow::ApplyForcedBreak(LayoutUnit logical_offset,
                                             EBreakBetween break_value) {
  NOT_DESTROYED();
  if (!IsForcedFragmentainerBreakValue(break_value))
    return logical_offset;
  if (!IsPageLogicalHeightKnown()) {
    // Page height is still unknown, so we cannot insert forced breaks.
    return logical_offset;
  }
  LayoutUnit remaining_logical_height = PageRemainingLogicalHeightForOffset(
      logical_offset, kAssociateWithLatterPage);
  if (remaining_logical_height == PageLogicalHeightForOffset(logical_offset))
    return logical_offset;  // Don't break if we're already at the block start
                            // of a fragmentainer.

  // If this is the first forced break inside this object, store the
  // location. We need this information later if there's a break-inside:avoid
  // object further up. We need to know if there are any forced breaks inside
  // such objects, in order to determine whether we need to push it to the next
  // fragmentainer or not.
  if (!FirstForcedBreakOffset())
    SetFirstForcedBreakOffset(logical_offset);

  return logical_offset + remaining_logical_height;
}

void LayoutBlockFlow::SetBreakBefore(EBreakBetween break_value) {
  NOT_DESTROYED();
  if (break_value != EBreakBetween::kAuto &&
      !IsBreakBetweenControllable(break_value))
    break_value = EBreakBetween::kAuto;
  if (break_value == EBreakBetween::kAuto && !rare_data_)
    return;
  EnsureRareData().break_before_ = static_cast<unsigned>(break_value);
}

void LayoutBlockFlow::SetBreakAfter(EBreakBetween break_value) {
  NOT_DESTROYED();
  if (break_value != EBreakBetween::kAuto &&
      !IsBreakBetweenControllable(break_value))
    break_value = EBreakBetween::kAuto;
  if (break_value == EBreakBetween::kAuto && !rare_data_)
    return;
  EnsureRareData().break_after_ = static_cast<unsigned>(break_value);
}

EBreakBetween LayoutBlockFlow::BreakBefore() const {
  NOT_DESTROYED();
  return rare_data_ ? static_cast<EBreakBetween>(rare_data_->break_before_)
                    : EBreakBetween::kAuto;
}

EBreakBetween LayoutBlockFlow::BreakAfter() const {
  NOT_DESTROYED();
  return rare_data_ ? static_cast<EBreakBetween>(rare_data_->break_after_)
                    : EBreakBetween::kAuto;
}

void LayoutBlockFlow::AddVisualOverflowFromFloats(
    const NGPhysicalFragment& fragment) {
  NOT_DESTROYED();
  DCHECK(!NeedsLayout());
  DCHECK(!ChildPrePaintBlockedByDisplayLock());
  DCHECK(fragment.HasFloatingDescendantsForPaint());

  for (const NGLink& child : fragment.PostLayoutChildren()) {
    if (child->HasSelfPaintingLayer())
      continue;

    if (child->IsFloating()) {
      AddVisualOverflowFromChild(To<LayoutBox>(*child->GetLayoutObject()));
      continue;
    }

    if (const NGPhysicalFragment* child_container = child.get()) {
      if (child_container->HasFloatingDescendantsForPaint() &&
          !child_container->IsFormattingContextRoot())
        AddVisualOverflowFromFloats(*child_container);
    }
  }
}

void LayoutBlockFlow::ComputeVisualOverflow(bool recompute_floats) {
  NOT_DESTROYED();
  DCHECK(!SelfNeedsLayout());

  LayoutRect previous_visual_overflow_rect = VisualOverflowRectAllowingUnset();
  ClearVisualOverflow();
  AddVisualOverflowFromChildren();
  AddVisualEffectOverflow();

  if (VisualOverflowRect() != previous_visual_overflow_rect) {
    InvalidateIntersectionObserverCachedRects();
    SetShouldCheckForPaintInvalidation();
    GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);
  }
}

RootInlineBox* LayoutBlockFlow::CreateAndAppendRootInlineBox() {
  NOT_DESTROYED();
  RootInlineBox* root_box = CreateRootInlineBox();
  line_boxes_.AppendLineBox(root_box);

  return root_box;
}

// Note: When this function is called from |LayoutInline::SplitFlow()|, some
// fragments point to destroyed |LayoutObject|.
void LayoutBlockFlow::DeleteLineBoxTree() {
  NOT_DESTROYED();

  line_boxes_.DeleteLineBoxTree();
}

int LayoutBlockFlow::LineCount(
    const RootInlineBox* stop_root_inline_box) const {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  DCHECK(!stop_root_inline_box ||
         stop_root_inline_box->Block().DebugPointer() == this);
#endif
  if (!ChildrenInline())
    return 0;

  int count = 0;
  for (const RootInlineBox* box = FirstRootBox(); box;
       box = box->NextRootBox()) {
    count++;
    if (box == stop_root_inline_box)
      break;
  }
  return count;
}

LayoutUnit LayoutBlockFlow::FirstLineBoxBaseline() const {
  NOT_DESTROYED();
  if (!ChildrenInline())
    return LayoutBlock::FirstLineBoxBaseline();
  if (const absl::optional<LayoutUnit> baseline =
          FirstLineBoxBaselineOverride())
    return *baseline;
  if (FirstLineBox()) {
    const SimpleFontData* font_data = Style(true)->GetFont().PrimaryFont();
    DCHECK(font_data);
    if (!font_data)
      return LayoutUnit(-1);
    // fontMetrics 'ascent' is the distance above the baseline to the 'over'
    // edge, which is 'top' for horizontal and 'right' for vertical-lr and
    // vertical-rl. However, firstLineBox()->logicalTop() gives the offset from
    // the 'left' edge for vertical-lr, hence we need to use the Font Metrics
    // 'descent' instead. The result should be handled accordingly by the caller
    // as a 'descent' value, in order to compute properly the max baseline.
    if (StyleRef().IsFlippedLinesWritingMode()) {
      return FirstLineBox()->LogicalTop() + font_data->GetFontMetrics().Descent(
                                                FirstRootBox()->BaselineType());
    }
    return FirstLineBox()->LogicalTop() +
           font_data->GetFontMetrics().Ascent(FirstRootBox()->BaselineType());
  }
  return EmptyLineBaseline(IsHorizontalWritingMode() ? kHorizontalLine
                                                     : kVerticalLine);
}

LayoutUnit LayoutBlockFlow::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  NOT_DESTROYED();
  if (!ChildrenInline())
    return LayoutBlock::InlineBlockBaseline(line_direction);
  if (const absl::optional<LayoutUnit> baseline =
          InlineBlockBaselineOverride(line_direction))
    return *baseline;
  if (LastLineBox()) {
    const SimpleFontData* font_data =
        Style(LastLineBox() == FirstLineBox())->GetFont().PrimaryFont();
    DCHECK(font_data);
    if (!font_data)
      return LayoutUnit(-1);
    // InlineFlowBox::placeBoxesInBlockDirection will flip lines in
    // case of verticalLR mode, so we can assume verticalRL for now.
    if (StyleRef().IsFlippedLinesWritingMode()) {
      return LogicalHeight() - LastLineBox()->LogicalBottom() +
             font_data->GetFontMetrics().Ascent(LastRootBox()->BaselineType());
    }
    return LastLineBox()->LogicalTop() +
           font_data->GetFontMetrics().Ascent(LastRootBox()->BaselineType());
  }
  return EmptyLineBaseline(line_direction);
}

void LayoutBlockFlow::WillBeDestroyed() {
  NOT_DESTROYED();
  // Make sure to destroy anonymous children first while they are still
  // connected to the rest of the tree, so that they will properly dirty line
  // boxes that they are removed from. Effects that do :before/:after only on
  // hover could crash otherwise.
  Children()->DestroyLeftoverChildren();

  if (!DocumentBeingDestroyed()) {
    // TODO(mstensho): figure out if we need this. We have no test coverage for
    // it. It looks like all line boxes have been removed at this point.
    if (FirstLineBox()) {
      // If we are an anonymous block, then our line boxes might have children
      // that will outlast this block. In the non-anonymous block case those
      // children will be destroyed by the time we return from this function.
      if (IsAnonymousBlock()) {
        for (InlineFlowBox* box : *LineBoxes()) {
          while (InlineBox* child_box = box->FirstChild())
            child_box->Remove();
        }
      }
    }
  }

  line_boxes_.DeleteLineBoxes();

  LayoutBlock::WillBeDestroyed();
}

void LayoutBlockFlow::UpdateBlockChildDirtyBitsBeforeLayout(
    bool relayout_children,
    LayoutBox& child) {
  NOT_DESTROYED();
  if (auto* placeholder = DynamicTo<LayoutMultiColumnSpannerPlaceholder>(child))
    placeholder->MarkForLayoutIfObjectInFlowThreadNeedsLayout();
  LayoutBlock::UpdateBlockChildDirtyBitsBeforeLayout(relayout_children, child);
}

void LayoutBlockFlow::UpdateStaticInlinePositionForChild(
    LayoutBox& child,
    LayoutUnit logical_top,
    IndentTextOrNot indent_text) {
  NOT_DESTROYED();
  if (child.StyleRef().IsOriginalDisplayInlineType())
    SetStaticInlinePositionForChild(
        child, StartAlignedOffsetForLine(logical_top, indent_text));
  else
    SetStaticInlinePositionForChild(child, StartOffsetForContent());
}

void LayoutBlockFlow::SetStaticInlinePositionForChild(
    LayoutBox& child,
    LayoutUnit inline_position) {
  NOT_DESTROYED();
  child.Layer()->SetStaticInlinePosition(inline_position);
}

void LayoutBlockFlow::AddChild(LayoutObject* new_child,
                               LayoutObject* before_child) {
  NOT_DESTROYED();
  if (LayoutMultiColumnFlowThread* flow_thread = MultiColumnFlowThread()) {
    if (before_child == flow_thread)
      before_child = flow_thread->FirstChild();
    DCHECK(!before_child || before_child->IsDescendantOf(flow_thread));
    flow_thread->AddChild(new_child, before_child);
    return;
  }

  if (before_child && before_child->Parent() != this) {
    AddChildBeforeDescendant(new_child, before_child);
    return;
  }

  bool made_boxes_non_inline = false;

  // A block has to either have all of its children inline, or all of its
  // children as blocks.
  // So, if our children are currently inline and a block child has to be
  // inserted, we move all our inline children into anonymous block boxes.
  bool child_is_block_level =
      !new_child->IsInline() && !new_child->IsFloatingOrOutOfFlowPositioned();

  if (ChildrenInline()) {
    if (child_is_block_level) {
      // Wrap the inline content in anonymous blocks, to allow for the new block
      // child to be inserted.
      MakeChildrenNonInline(before_child);
      made_boxes_non_inline = true;

      if (before_child && before_child->Parent() != this) {
        before_child = before_child->Parent();
        DCHECK(before_child->IsAnonymousBlock());
        DCHECK_EQ(before_child->Parent(), this);
      }
    }
  } else if (!child_is_block_level) {
    // This block has block children. We may want to put the new child into an
    // anomyous block. Floats and out-of-flow children may live among either
    // block or inline children, so for such children, only put them inside an
    // anonymous block if one already exists. If the child is inline, on the
    // other hand, we *have to* put it inside an anonymous block, so create a
    // new one if there is none for us there already.
    LayoutObject* after_child =
        before_child ? before_child->PreviousSibling() : LastChild();

    if (after_child && after_child->IsAnonymousBlock()) {
      after_child->AddChild(new_child);
      return;
    }

    // LayoutNGOutsideListMarker is out-of-flow for the tree building purpose,
    // and that is not inline level, but IsInline().
    if (new_child->IsInline() && !new_child->IsLayoutNGOutsideListMarker()) {
      // No suitable existing anonymous box - create a new one.
      auto* new_block = To<LayoutBlockFlow>(CreateAnonymousBlock());
      LayoutBox::AddChild(new_block, before_child);
      // Reparent adjacent floating or out-of-flow siblings to the new box.
      new_block->ReparentPrecedingFloatingOrOutOfFlowSiblings();
      new_block->AddChild(new_child);
      new_block->ReparentSubsequentFloatingOrOutOfFlowSiblings();
      return;
    }
  }

  // Skip the LayoutBlock override, since that one deals with anonymous child
  // insertion in a way that isn't sufficient for us, and can only cause trouble
  // at this point.
  LayoutBox::AddChild(new_child, before_child);
  auto* parent_layout_block = DynamicTo<LayoutBlock>(Parent());
  if (made_boxes_non_inline && IsAnonymousBlock() && parent_layout_block) {
    parent_layout_block->RemoveLeftoverAnonymousBlock(this);
    // |this| may be dead now.
  }
}

static bool IsMergeableAnonymousBlock(const LayoutBlockFlow* block) {
  return block->IsAnonymousBlock() && !block->BeingDestroyed() &&
         !block->IsRubyRun() && !block->IsRubyBase();
}

void LayoutBlockFlow::RemoveChild(LayoutObject* old_child) {
  NOT_DESTROYED();
  // No need to waste time in merging or removing empty anonymous blocks.
  // We can just bail out if our document is getting destroyed.
  if (DocumentBeingDestroyed()) {
    LayoutBox::RemoveChild(old_child);
    return;
  }

  // If this child is a block, and if our previous and next siblings are both
  // anonymous blocks with inline content, then we can go ahead and fold the
  // inline content back together. If only one of the siblings is such an
  // anonymous blocks, check if the other sibling (and any of *its* siblings)
  // are floating or out-of-flow positioned. In that case, they should be moved
  // into the anonymous block.
  LayoutObject* prev = old_child->PreviousSibling();
  LayoutObject* next = old_child->NextSibling();
  bool merged_anonymous_blocks = false;
  if (prev && next && !old_child->IsInline()) {
    auto* prev_block_flow = DynamicTo<LayoutBlockFlow>(prev);
    auto* next_block_flow = DynamicTo<LayoutBlockFlow>(next);
    if (prev_block_flow && next_block_flow &&
        prev_block_flow->MergeSiblingContiguousAnonymousBlock(
            next_block_flow)) {
      merged_anonymous_blocks = true;
      next = nullptr;
    } else if (prev_block_flow && IsMergeableAnonymousBlock(prev_block_flow)) {
      // The previous sibling is anonymous. Scan the next siblings and reparent
      // any floating or out-of-flow positioned objects into the end of the
      // previous anonymous block.
      while (next && next->IsFloatingOrOutOfFlowPositioned()) {
        LayoutObject* sibling = next->NextSibling();
        MoveChildTo(prev_block_flow, next, nullptr, false);
        next = sibling;
      }
    } else if (next_block_flow && IsMergeableAnonymousBlock(next_block_flow)) {
      // The next sibling is anonymous. Scan the previous siblings and reparent
      // any floating or out-of-flow positioned objects into the start of the
      // next anonymous block.
      while (prev && prev->IsFloatingOrOutOfFlowPositioned()) {
        LayoutObject* sibling = prev->PreviousSibling();
        MoveChildTo(next_block_flow, prev, next_block_flow->FirstChild(),
                    false);
        prev = sibling;
      }
    }
  }

  LayoutBlock::RemoveChild(old_child);

  LayoutObject* child = prev ? prev : next;
  auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
  if (child && child_block_flow && !child->PreviousSibling() &&
      !child->NextSibling()) {
    // If the removal has knocked us down to containing only a single anonymous
    // box we can go ahead and pull the content right back up into our
    // box.
    if (merged_anonymous_blocks || IsMergeableAnonymousBlock(child_block_flow))
      CollapseAnonymousBlockChild(child_block_flow);
  }

  if (!FirstChild()) {
    // If this was our last child be sure to clear out our line boxes.
    if (ChildrenInline())
      DeleteLineBoxTree();
  } else if (!BeingDestroyed() &&
             !old_child->IsFloatingOrOutOfFlowPositioned() &&
             !old_child->IsAnonymousBlock()) {
    // If the child we're removing means that we can now treat all children as
    // inline without the need for anonymous blocks, then do that.
    MakeChildrenInlineIfPossible();
  }
}

bool LayoutBlockFlow::CreatesAnonymousWrapper() const {
  return IsLayoutFlowThread() && Parent()->IsLayoutNGObject();
}

void LayoutBlockFlow::MoveAllChildrenIncludingFloatsTo(
    LayoutBlock* to_block,
    bool full_remove_insert) {
  NOT_DESTROYED();
  auto* to_block_flow = To<LayoutBlockFlow>(to_block);

  DCHECK(full_remove_insert ||
         to_block_flow->ChildrenInline() == ChildrenInline());

  MoveAllChildrenTo(to_block_flow, full_remove_insert);
}

void LayoutBlockFlow::ChildBecameFloatingOrOutOfFlow(LayoutBox* child) {
  NOT_DESTROYED();
  MakeChildrenInlineIfPossible();

  // Reparent the child to an adjacent anonymous block if one is available.
  LayoutObject* prev = child->PreviousSibling();
  auto* new_container = DynamicTo<LayoutBlockFlow>(prev);
  if (prev && prev->IsAnonymousBlock() && new_container) {
    MoveChildTo(new_container, child, nullptr, false);
    // The anonymous block we've moved to may now be adjacent to former siblings
    // of ours that it can contain also.
    new_container->ReparentSubsequentFloatingOrOutOfFlowSiblings();
    return;
  }
  LayoutObject* next = child->NextSibling();
  new_container = DynamicTo<LayoutBlockFlow>(next);
  if (next && next->IsAnonymousBlock() && next->IsLayoutBlockFlow()) {
    MoveChildTo(new_container, child, new_container->FirstChild(), false);
  }
}

static bool AllowsCollapseAnonymousBlockChild(const LayoutBlockFlow& parent,
                                              const LayoutBlockFlow& child) {
  // It's possible that this block's destruction may have been triggered by the
  // child's removal. Just bail if the anonymous child block is already being
  // destroyed. See crbug.com/282088
  if (child.BeingDestroyed())
    return false;
  // Ruby elements use anonymous wrappers for ruby runs and ruby bases by
  // design, so we don't remove them.
  if (child.IsRubyRun() || child.IsRubyBase())
    return false;
  if (IsA<LayoutMultiColumnFlowThread>(parent) &&
      parent.Parent()->IsLayoutNGObject() && child.ChildrenInline()) {
    // The test[1] reaches here.
    // [1] "fast/multicol/dynamic/remove-spanner-in-content.html"
    return false;
  }
  return true;
}

void LayoutBlockFlow::CollapseAnonymousBlockChild(LayoutBlockFlow* child) {
  NOT_DESTROYED();
  if (!AllowsCollapseAnonymousBlockChild(*this, *child))
    return;
  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kChildAnonymousBlockChanged);

  child->MoveAllChildrenTo(this, child->NextSibling(), child->HasLayer());
  SetChildrenInline(child->ChildrenInline());

  Children()->RemoveChildNode(this, child, child->HasLayer());
  child->Destroy();
}

bool LayoutBlockFlow::MergeSiblingContiguousAnonymousBlock(
    LayoutBlockFlow* sibling_that_may_be_deleted) {
  NOT_DESTROYED();
  // Note: |this| and |siblingThatMayBeDeleted| may not be adjacent siblings at
  // this point. There may be an object between them which is about to be
  // removed.

  if (!IsMergeableAnonymousBlock(this) ||
      !IsMergeableAnonymousBlock(sibling_that_may_be_deleted))
    return false;

  SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kAnonymousBlockChange);

  // If the inlineness of children of the two block don't match, we'd need
  // special code here (but there should be no need for it).
  DCHECK_EQ(sibling_that_may_be_deleted->ChildrenInline(), ChildrenInline());

  // Take all the children out of the |next| block and put them in the |prev|
  // block. If there are paint layers involved, or if we're part of a flow
  // thread, we need to notify the layout tree about the movement.
  bool full_remove_insert = sibling_that_may_be_deleted->HasLayer() ||
                            HasLayer() ||
                            sibling_that_may_be_deleted->IsInsideFlowThread();
  sibling_that_may_be_deleted->MoveAllChildrenIncludingFloatsTo(
      this, full_remove_insert);
  // Delete the now-empty block's lines and nuke it.
  sibling_that_may_be_deleted->DeleteLineBoxTree();
  sibling_that_may_be_deleted->Destroy();
  return true;
}

void LayoutBlockFlow::ReparentSubsequentFloatingOrOutOfFlowSiblings() {
  NOT_DESTROYED();
  auto* parent_block_flow = DynamicTo<LayoutBlockFlow>(Parent());
  if (!parent_block_flow)
    return;
  if (BeingDestroyed() || DocumentBeingDestroyed())
    return;
  LayoutObject* child = NextSibling();
  while (child && child->IsFloatingOrOutOfFlowPositioned()) {
    LayoutObject* sibling = child->NextSibling();
    parent_block_flow->MoveChildTo(this, child, nullptr, false);
    child = sibling;
  }

  if (LayoutObject* next = NextSibling()) {
    auto* next_block_flow = DynamicTo<LayoutBlockFlow>(next);
    if (next_block_flow)
      MergeSiblingContiguousAnonymousBlock(next_block_flow);
  }
}

void LayoutBlockFlow::ReparentPrecedingFloatingOrOutOfFlowSiblings() {
  NOT_DESTROYED();
  auto* parent_block_flow = DynamicTo<LayoutBlockFlow>(Parent());
  if (!parent_block_flow)
    return;
  if (BeingDestroyed() || DocumentBeingDestroyed())
    return;
  LayoutObject* child = PreviousSibling();
  while (child && child->IsFloatingOrOutOfFlowPositioned()) {
    LayoutObject* sibling = child->PreviousSibling();
    parent_block_flow->MoveChildTo(this, child, FirstChild(), false);
    child = sibling;
  }
}

static bool AllowsInlineChildren(const LayoutBlockFlow& block_flow) {
  // Collapsing away anonymous wrappers isn't relevant for the children of
  // anonymous blocks, unless they are ruby bases.
  if (block_flow.IsAnonymousBlock() && !block_flow.IsRubyBase())
    return false;
  if (IsA<LayoutMultiColumnFlowThread>(block_flow) &&
      block_flow.Parent()->IsLayoutNGObject())
    return false;
  return true;
}

void LayoutBlockFlow::MakeChildrenInlineIfPossible() {
  NOT_DESTROYED();
  if (!AllowsInlineChildren(*this))
    return;

  HeapVector<Member<LayoutBlockFlow>, 3> blocks_to_remove;
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsFloating())
      continue;
    if (child->IsOutOfFlowPositioned())
      continue;

    // There are still block children in the container, so any anonymous
    // wrappers are still needed.
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (!child->IsAnonymousBlock() || !child_block_flow)
      return;
    // If one of the children is being destroyed then it is unsafe to clean up
    // anonymous wrappers as the
    // entire branch may be being destroyed.
    if (child_block_flow->BeingDestroyed())
      return;
    // We are only interested in removing anonymous wrappers if there are inline
    // siblings underneath them.
    if (!child->ChildrenInline())
      return;
    // Ruby elements use anonymous wrappers for ruby runs and ruby bases by
    // design, so we don't remove them.
    if (child->IsRubyRun() || child->IsRubyBase())
      return;

    blocks_to_remove.push_back(child_block_flow);
  }

  for (LayoutBlockFlow* child : blocks_to_remove)
    CollapseAnonymousBlockChild(child);
  SetChildrenInline(true);
}

static void GetInlineRun(LayoutObject* start,
                         LayoutObject* boundary,
                         LayoutObject*& inline_run_start,
                         LayoutObject*& inline_run_end) {
  // Beginning at |start| we find the largest contiguous run of inlines that
  // we can.  We denote the run with start and end points, |inlineRunStart|
  // and |inlineRunEnd|.  Note that these two values may be the same if
  // we encounter only one inline.
  //
  // We skip any non-inlines we encounter as long as we haven't found any
  // inlines yet.
  //
  // |boundary| indicates a non-inclusive boundary point.  Regardless of whether
  // |boundary| is inline or not, we will not include it in a run with inlines
  // before it. It's as though we encountered a non-inline.

  // Start by skipping as many non-inlines as we can.
  LayoutObject* curr = start;

  // LayoutNGOutsideListMarker is out-of-flow for the tree building purpose.
  // Skip here because it's the first child.
  if (curr && curr->IsLayoutNGOutsideListMarker())
    curr = curr->NextSibling();

  bool saw_inline;
  do {
    while (curr &&
           !(curr->IsInline() || curr->IsFloatingOrOutOfFlowPositioned()))
      curr = curr->NextSibling();

    inline_run_start = inline_run_end = curr;

    if (!curr)
      return;  // No more inline children to be found.

    saw_inline = curr->IsInline();

    curr = curr->NextSibling();
    while (curr &&
           (curr->IsInline() || curr->IsFloatingOrOutOfFlowPositioned()) &&
           (curr != boundary)) {
      inline_run_end = curr;
      if (curr->IsInline())
        saw_inline = true;
      curr = curr->NextSibling();
    }
  } while (!saw_inline);
}

void LayoutBlockFlow::MakeChildrenNonInline(LayoutObject* insertion_point) {
  NOT_DESTROYED();

  // makeChildrenNonInline takes a block whose children are *all* inline and it
  // makes sure that inline children are coalesced under anonymous blocks.
  // If |insertionPoint| is defined, then it represents the insertion point for
  // the new block child that is causing us to have to wrap all the inlines.
  // This means that we cannot coalesce inlines before |insertionPoint| with
  // inlines following |insertionPoint|, because the new child is going to be
  // inserted in between the inlines, splitting them.
  DCHECK(!IsInline() || IsAtomicInlineLevel());
  DCHECK(!insertion_point || insertion_point->Parent() == this);

  SetChildrenInline(false);
  ClearNGInlineNodeData();

  LayoutObject* child = FirstChild();
  if (!child)
    return;

  DeleteLineBoxTree();

  while (child) {
    LayoutObject* inline_run_start;
    LayoutObject* inline_run_end;
    GetInlineRun(child, insertion_point, inline_run_start, inline_run_end);

    if (!inline_run_start)
      break;

    child = inline_run_end->NextSibling();

    LayoutBlock* block = CreateAnonymousBlock();
    Children()->InsertChildNode(this, block, inline_run_start);
    MoveChildrenTo(block, inline_run_start, child);
  }

#if DCHECK_IS_ON()
  for (LayoutObject* c = FirstChild(); c; c = c->NextSibling())
    DCHECK(!c->IsInline() || c->IsLayoutNGOutsideListMarker());
#endif

  SetShouldDoFullPaintInvalidation();
}

void LayoutBlockFlow::ChildBecameNonInline(LayoutObject*) {
  NOT_DESTROYED();
  MakeChildrenNonInline();
  auto* parent_layout_block = DynamicTo<LayoutBlock>(Parent());
  if (IsAnonymousBlock() && parent_layout_block)
    parent_layout_block->RemoveLeftoverAnonymousBlock(this);
  // |this| may be dead here
}

LayoutUnit LayoutBlockFlow::LogicalLeftOffsetForPositioningFloat(
    LayoutUnit logical_top,
    LayoutUnit fixed_offset,
    LayoutUnit* height_remaining) const {
  NOT_DESTROYED();
  LayoutUnit offset = fixed_offset;
  return AdjustLogicalLeftOffsetForLine(offset, kDoNotIndentText);
}

LayoutUnit LayoutBlockFlow::LogicalRightOffsetForPositioningFloat(
    LayoutUnit logical_top,
    LayoutUnit fixed_offset,
    LayoutUnit* height_remaining) const {
  NOT_DESTROYED();
  LayoutUnit offset = fixed_offset;
  return AdjustLogicalRightOffsetForLine(offset, kDoNotIndentText);
}

LayoutUnit LayoutBlockFlow::AdjustLogicalLeftOffsetForLine(
    LayoutUnit offset_from_floats,
    IndentTextOrNot apply_text_indent) const {
  NOT_DESTROYED();
  LayoutUnit left = offset_from_floats;

  if (apply_text_indent == kIndentText && StyleRef().IsLeftToRightDirection())
    left += TextIndentOffset();

  return left;
}

LayoutUnit LayoutBlockFlow::AdjustLogicalRightOffsetForLine(
    LayoutUnit offset_from_floats,
    IndentTextOrNot apply_text_indent) const {
  NOT_DESTROYED();
  LayoutUnit right = offset_from_floats;

  if (apply_text_indent == kIndentText && !StyleRef().IsLeftToRightDirection())
    right -= TextIndentOffset();

  return right;
}

Node* LayoutBlockFlow::NodeForHitTest() const {
  NOT_DESTROYED();
  // If we are in the margins of block elements that are part of a
  // block-in-inline we're actually still inside the enclosing element
  // that was split. Use the appropriate inner node.
  if (UNLIKELY(IsBlockInInline())) {
    DCHECK(Parent());
    DCHECK(Parent()->IsLayoutInline());
    return Parent()->NodeForHitTest();
  }
  return LayoutBlock::NodeForHitTest();
}

bool LayoutBlockFlow::HitTestChildren(HitTestResult& result,
                                      const HitTestLocation& hit_test_location,
                                      const PhysicalOffset& accumulated_offset,
                                      HitTestPhase phase) {
  NOT_DESTROYED();
  PhysicalOffset scrolled_offset = accumulated_offset;
  if (IsScrollContainer())
    scrolled_offset -= PhysicalOffset(PixelSnappedScrolledContentOffset());

  if (ChildrenInline()) {
    if (line_boxes_.HitTest(LineLayoutBoxModel(this), result, hit_test_location,
                            scrolled_offset, phase)) {
      UpdateHitTestResult(result,
                          hit_test_location.Point() - accumulated_offset);
      return true;
    }
  } else if (LayoutBlock::HitTestChildren(result, hit_test_location,
                                          accumulated_offset, phase)) {
    return true;
  }

  return false;
}

bool LayoutBlockFlow::AllowsColumns() const {
  // Ruby elements manage child insertion in a special way, and would mess up
  // insertion of the flow thread. The flow thread needs to be a direct child of
  // the multicol block (|this|).
  if (IsRuby())
    return false;

  // We don't allow custom layout and multicol on the same object. This is
  // similar to not allowing it for flexbox, grids and tables (although those
  // don't create LayoutBlockFlow, so we don't need to check for those here).
  if (StyleRef().IsDisplayLayoutCustomBox())
    return false;

  // MathML layout objects don't support multicol.
  if (IsMathML())
    return false;

  return true;
}

bool LayoutBlockFlow::AllowsPaginationStrut() const {
  NOT_DESTROYED();
  // The block needs to be contained by a LayoutBlockFlow (and not by e.g. a
  // flexbox, grid, or a table (the latter being the case for table cell or
  // table caption)). The reason for this limitation is simply that
  // LayoutBlockFlow child layout code is the only place where we pick up the
  // struts and handle them. We handle floats and regular in-flow children, and
  // that's all. We could handle this in other layout modes as well (and even
  // for out-of-flow children), but currently we don't.
  if (IsOutOfFlowPositioned())
    return false;
  if (IsLayoutFlowThread()) {
    // Don't let the strut escape the fragmentation context and get lost.
    return false;
  }
  const auto* containing_block_flow =
      DynamicTo<LayoutBlockFlow>(ContainingBlock());
  if (!containing_block_flow)
    return false;
  // If children are inline, allow the strut. We are probably a float.
  if (containing_block_flow->ChildrenInline())
    return true;
  for (LayoutBox* sibling = PreviousSiblingBox(); sibling;
       sibling = sibling->PreviousSiblingBox()) {
    // What happens on the other side of a spanner is none of our concern, so
    // stop here. Since there's no in-flow box between the previous spanner and
    // us, there's no class A break point in front of us. We cannot even
    // re-propagate pagination struts to our containing block, since the
    // containing block starts in a different column row.
    if (sibling->IsColumnSpanAll())
      return false;
    // If this isn't the first in-flow object, there's a break opportunity
    // before us, which means that we can allow the strut.
    if (!sibling->IsFloatingOrOutOfFlowPositioned())
      return true;
  }
  // This is a first in-flow child. We'll still allow the strut if it can be
  // re-propagated to our containing block.
  return containing_block_flow->AllowsPaginationStrut();
}

void LayoutBlockFlow::SetPaginationStrutPropagatedFromChild(LayoutUnit strut) {
  NOT_DESTROYED();
  strut = std::max(strut, LayoutUnit());
  if (!rare_data_) {
    if (!strut)
      return;
    rare_data_ = MakeGarbageCollected<LayoutBlockFlowRareData>(this);
  }
  rare_data_->pagination_strut_propagated_from_child_ = strut;
}

void LayoutBlockFlow::SetFirstForcedBreakOffset(LayoutUnit block_offset) {
  NOT_DESTROYED();
  if (!rare_data_) {
    if (!block_offset)
      return;
    rare_data_ = MakeGarbageCollected<LayoutBlockFlowRareData>(this);
  }
  rare_data_->first_forced_break_offset_ = block_offset;
}

void LayoutBlockFlow::PositionSpannerDescendant(
    LayoutMultiColumnSpannerPlaceholder& child) {
  NOT_DESTROYED();
  LayoutBox& spanner = *child.LayoutObjectInFlowThread();
  // FIXME: |spanner| is a descendant, but never a direct child, so the names
  // here are bad, if nothing else.
  SetLogicalTopForChild(spanner, child.LogicalTop());
  DetermineLogicalLeftPositionForChild(spanner);
}

void LayoutBlockFlow::MoveChildrenTo(LayoutBoxModelObject* to_box_model_object,
                                     LayoutObject* start_child,
                                     LayoutObject* end_child,
                                     LayoutObject* before_child,
                                     bool full_remove_insert) {
  NOT_DESTROYED();
  if (ChildrenInline())
    DeleteLineBoxTree();
  LayoutBoxModelObject::MoveChildrenTo(to_box_model_object, start_child,
                                       end_child, before_child,
                                       full_remove_insert);
}

RootInlineBox* LayoutBlockFlow::CreateRootInlineBox() {
  NOT_DESTROYED();
  return MakeGarbageCollected<RootInlineBox>(LineLayoutItem(this));
}

void LayoutBlockFlow::CreateOrDestroyMultiColumnFlowThreadIfNeeded(
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  bool specifies_columns = StyleRef().SpecifiesColumns();

  if (MultiColumnFlowThread()) {
    DCHECK(old_style);
    if (specifies_columns != old_style->SpecifiesColumns()) {
      // If we're no longer to be multicol/paged, destroy the flow thread. Also
      // destroy it when switching between multicol and paged, since that
      // affects the column set structure (multicol containers may have
      // spanners, paged containers may not).
      MultiColumnFlowThread()->EvacuateAndDestroy();
      DCHECK(!MultiColumnFlowThread());
      pagination_state_changed_ = true;
    }
    return;
  }

  if (!specifies_columns)
    return;

  if (IsListItemIncludingNG())
    UseCounter::Count(GetDocument(), WebFeature::kMultiColAndListItem);

  if (!AllowsColumns())
    return;

  // Fieldsets look for a legend special child (layoutSpecialExcludedChild()).
  // We currently only support one special child per layout object, and the
  // flow thread would make for a second one.
  // For LayoutNG, the multi-column display type will be applied to the
  // anonymous content box. Thus, the flow thread should be added to the
  // anonymous content box instead of the fieldset itself.
  if (IsFieldset()) {
    return;
  }

  // Form controls are replaced content (also when implemented as a regular
  // block), and are therefore not supposed to support multicol.
  const auto* element = DynamicTo<Element>(GetNode());
  if (element && element->IsFormControlElement())
    return;

  auto* flow_thread = LayoutMultiColumnFlowThread::CreateAnonymous(
      GetDocument(), StyleRef(), !IsLayoutNGObject());
  AddChild(flow_thread);
  pagination_state_changed_ = true;
  if (IsLayoutNGObject()) {
    // For simplicity of layout algorithm, we assume flow thread having block
    // level children only.
    // For example, we can handle them in same way:
    //   <div style="columns:3">abc<br>def<br>ghi<br></div>
    //   <div style="columns:3"><div>abc<br>def<br>ghi<br></div></div>
    flow_thread->SetChildrenInline(false);
  }

  // Check that addChild() put the flow thread as a direct child, and didn't do
  // fancy things.
  DCHECK_EQ(flow_thread->Parent(), this);

  flow_thread->Populate();

  LayoutBlockFlowRareData& rare_data = EnsureRareData();
  DCHECK(!rare_data.multi_column_flow_thread_);
  rare_data.multi_column_flow_thread_ = flow_thread;
}

LayoutBlockFlow::LayoutBlockFlowRareData& LayoutBlockFlow::EnsureRareData() {
  NOT_DESTROYED();
  if (rare_data_)
    return *rare_data_;

  rare_data_ = MakeGarbageCollected<LayoutBlockFlowRareData>(this);
  return *rare_data_;
}

void LayoutBlockFlow::SimplifiedNormalFlowInlineLayout() {
  NOT_DESTROYED();
  DCHECK(ChildrenInline());
  HeapLinkedHashSet<Member<RootInlineBox>> line_boxes;
  ClearCollectionScope<HeapLinkedHashSet<Member<RootInlineBox>>> scope(
      &line_boxes);
  for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.AtEnd();
       walker.Advance()) {
    LayoutObject* o = walker.Current().GetLayoutObject();
    if (!o->IsOutOfFlowPositioned() &&
        (o->IsAtomicInlineLevel() || o->IsFloating())) {
      o->LayoutIfNeeded();
      if (To<LayoutBox>(o)->InlineBoxWrapper()) {
        RootInlineBox& box = To<LayoutBox>(o)->InlineBoxWrapper()->Root();
        line_boxes.insert(&box);
      }
    } else if (o->IsText() ||
               (o->IsLayoutInline() && !walker.AtEndOfInline())) {
      o->ClearNeedsLayout();
    }
  }

  // FIXME: Glyph overflow will get lost in this case, but not really a big
  // deal.
  GlyphOverflowAndFallbackFontsMap text_box_data_map;
  for (auto box : line_boxes) {
    box->ComputeOverflow(box->LineTop(), box->LineBottom(), text_box_data_map);
  }
}

RecalcLayoutOverflowResult
LayoutBlockFlow::RecalcInlineChildrenLayoutOverflow() {
  NOT_DESTROYED();
  DCHECK(ChildrenInline());
  RecalcLayoutOverflowResult result;
  HeapHashSet<Member<RootInlineBox>> line_boxes;
  ClearCollectionScope<HeapHashSet<Member<RootInlineBox>>> scope(&line_boxes);
  for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.AtEnd();
       walker.Advance()) {
    LayoutObject* layout_object = walker.Current().GetLayoutObject();
    if (layout_object->IsOutOfFlowPositioned())
      continue;

    result.Unite(layout_object->RecalcLayoutOverflow());

    // TODO(chrishtr): should this be IsBox()? Non-blocks can be inline and
    // have line box wrappers.
    if (auto* layout_block_object = DynamicTo<LayoutBlock>(layout_object)) {
      if (InlineBox* inline_box_wrapper =
              layout_block_object->InlineBoxWrapper())
        line_boxes.insert(&inline_box_wrapper->Root());
    }
  }

  // FIXME: Glyph overflow will get lost in this case, but not really a big
  // deal.
  GlyphOverflowAndFallbackFontsMap text_box_data_map;
  for (auto box : line_boxes) {
    box->ClearKnownToHaveNoOverflow();
    box->ComputeOverflow(box->LineTop(), box->LineBottom(), text_box_data_map);
  }
  return result;
}

void LayoutBlockFlow::RecalcInlineChildrenVisualOverflow() {
  NOT_DESTROYED();
  DCHECK(ChildrenInline());

  // TODO(crbug.com/1144203): This code path should be switch to
  // |RecalcFragmentsVisualOverflow|.
  if (CanUseFragmentsForVisualOverflow()) {
    for (const NGPhysicalBoxFragment& fragment : PhysicalFragments()) {
      if (const NGFragmentItems* items = fragment.Items()) {
        NGInlineCursor cursor(fragment, *items);
        NGInlinePaintContext inline_context;
        NGFragmentItem::RecalcInkOverflowForCursor(&cursor, &inline_context);
      }
      // Even if this turned out to be an inline formatting context with
      // fragment items (handled above), we need to handle floating descendants.
      // If a float is block-fragmented, it is resumed as a regular box fragment
      // child, rather than becoming a fragment item.
      if (fragment.HasFloatingDescendantsForPaint())
        RecalcFloatingDescendantsVisualOverflow(fragment);
    }
    return;
  }

  for (InlineWalker walker(LineLayoutBlockFlow(this)); !walker.AtEnd();
       walker.Advance()) {
    LayoutObject* layout_object = walker.Current().GetLayoutObject();
    layout_object->RecalcNormalFlowChildVisualOverflowIfNeeded();
  }

  // Child inline boxes' self visual overflow is already computed at the same
  // time as layout overflow. But we need to add replaced children visual rects.
  for (RootInlineBox* box = FirstRootBox(); box; box = box->NextRootBox())
    box->AddReplacedChildrenVisualOverflow(box->LineTop(), box->LineBottom());
}

void LayoutBlockFlow::RecalcFloatingDescendantsVisualOverflow(
    const NGPhysicalFragment& fragment) {
  DCHECK(fragment.HasFloatingDescendantsForPaint());

  for (const NGLink& child : fragment.PostLayoutChildren()) {
    if (child->IsFloating()) {
      child->GetMutableLayoutObject()
          ->RecalcNormalFlowChildVisualOverflowIfNeeded();
      continue;
    }

    if (const NGPhysicalFragment* child_container_fragment = child.get()) {
      if (child_container_fragment->HasFloatingDescendantsForPaint())
        RecalcFloatingDescendantsVisualOverflow(*child_container_fragment);
    }
  }
}

PositionWithAffinity LayoutBlockFlow::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  // NG codepath requires |kPrePaintClean|.
  // |SelectionModifier| calls this only in legacy codepath.
  DCHECK(!IsLayoutNGObject() || GetDocument().Lifecycle().GetState() >=
                                    DocumentLifecycle::kPrePaintClean);

  if (IsAtomicInlineLevel()) {
    PositionWithAffinity position =
        PositionForPointIfOutsideAtomicInlineLevel(point);
    if (!position.IsNull())
      return position;
  }
  if (!ChildrenInline())
    return LayoutBlock::PositionForPoint(point);

  PhysicalOffset point_in_contents = point;
  OffsetForContents(point_in_contents);
  LayoutPoint point_in_logical_contents = FlipForWritingMode(point_in_contents);
  if (!IsHorizontalWritingMode())
    point_in_logical_contents = point_in_logical_contents.TransposedPoint();

  if (!FirstRootBox())
    return CreatePositionWithAffinity(0);

  bool lines_are_flipped = StyleRef().IsFlippedLinesWritingMode();
  bool blocks_are_flipped = StyleRef().IsFlippedBlocksWritingMode();

  // look for the closest line box in the root box which is at the passed-in y
  // coordinate
  InlineBox* closest_box = nullptr;
  RootInlineBox* first_root_box_with_children = nullptr;
  RootInlineBox* last_root_box_with_children = nullptr;
  for (RootInlineBox* root = FirstRootBox(); root; root = root->NextRootBox()) {
    if (!root->FirstLeafChild())
      continue;
    if (!first_root_box_with_children)
      first_root_box_with_children = root;

    if (!lines_are_flipped && root->IsFirstAfterPageBreak() &&
        (point_in_logical_contents.Y() < root->LineTopWithLeading() ||
         (blocks_are_flipped &&
          point_in_logical_contents.Y() == root->LineTopWithLeading())))
      break;

    last_root_box_with_children = root;

    // check if this root line box is located at this y coordinate
    if (point_in_logical_contents.Y() < root->SelectionBottom() ||
        (blocks_are_flipped &&
         point_in_logical_contents.Y() == root->SelectionBottom())) {
      if (lines_are_flipped) {
        RootInlineBox* next_root_box_with_children = root->NextRootBox();
        while (next_root_box_with_children &&
               !next_root_box_with_children->FirstLeafChild())
          next_root_box_with_children =
              next_root_box_with_children->NextRootBox();

        if (next_root_box_with_children &&
            next_root_box_with_children->IsFirstAfterPageBreak() &&
            (point_in_logical_contents.Y() >
                 next_root_box_with_children->LineTopWithLeading() ||
             (!blocks_are_flipped &&
              point_in_logical_contents.Y() ==
                  next_root_box_with_children->LineTopWithLeading())))
          continue;
      }
      closest_box = root->ClosestLeafChildForLogicalLeftPosition(
          point_in_logical_contents.X());
      if (closest_box)
        break;
    }
  }

  const bool move_caret_to_boundary =
      ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();
  if (!move_caret_to_boundary && !closest_box && last_root_box_with_children) {
    // y coordinate is below last root line box, pretend we hit it
    closest_box =
        last_root_box_with_children->ClosestLeafChildForLogicalLeftPosition(
            point_in_logical_contents.X());
  }

  if (closest_box) {
    if (move_caret_to_boundary) {
      LayoutUnit first_root_box_with_children_top =
          std::min<LayoutUnit>(first_root_box_with_children->SelectionTop(),
                               first_root_box_with_children->LogicalTop());
      if (point_in_logical_contents.Y() < first_root_box_with_children_top ||
          (blocks_are_flipped &&
           point_in_logical_contents.Y() == first_root_box_with_children_top)) {
        InlineBox* box = first_root_box_with_children->FirstLeafChild();
        if (box->IsLineBreak()) {
          if (InlineBox* new_box = box->NextLeafChildIgnoringLineBreak())
            box = new_box;
        }
        // y coordinate is above first root line box, so return the start of the
        // first
        return PositionWithAffinity(PositionForBox(box, true));
      }
    }

    if (closest_box->GetLineLayoutItem().IsAtomicInlineLevel()) {
      // We want to pass the original point other than a corrected one.
      LayoutPoint adjusted_point(point_in_logical_contents);
      if (!IsHorizontalWritingMode())
        adjusted_point = adjusted_point.TransposedPoint();
      return PositionForPointRespectingEditingBoundaries(
          LineLayoutBox(closest_box->GetLineLayoutItem()),
          FlipForWritingMode(adjusted_point));
    }

    // pass the box a top position that is inside it
    LayoutPoint adjusted_point(point_in_logical_contents.X(),
                               closest_box->Root().BlockDirectionPointInLine());
    if (!IsHorizontalWritingMode())
      adjusted_point = adjusted_point.TransposedPoint();
    return closest_box->GetLineLayoutItem().PositionForPoint(
        FlipForWritingMode(adjusted_point));
  }

  if (last_root_box_with_children) {
    // We hit this case for Mac behavior when the Y coordinate is below the last
    // box.
    DCHECK(move_caret_to_boundary);
    if (const InlineBox* logically_last_box =
            last_root_box_with_children->GetLogicalEndNonPseudoBox()) {
      // TODO(layout-dev): Change |PositionForBox()| to take |const InlineBox*|.
      return PositionWithAffinity(
          PositionForBox(const_cast<InlineBox*>(logically_last_box), false));
    }
  }

  // Can't reach this. We have a root line box, but it has no kids.
  // FIXME: This should NOTREACHED(), but clicking on placeholder text
  // seems to hit this code path.
  return CreatePositionWithAffinity(0);
}

bool LayoutBlockFlow::ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom()
    const {
  NOT_DESTROYED();
  return GetDocument()
      .GetFrame()
      ->GetEditor()
      .Behavior()
      .ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();
}

#if DCHECK_IS_ON()

void LayoutBlockFlow::ShowLineTreeAndMark(const InlineBox* marked_box1,
                                          const char* marked_label1,
                                          const InlineBox* marked_box2,
                                          const char* marked_label2,
                                          const LayoutObject* obj) const {
  NOT_DESTROYED();
  if (getenv("RUNNING_UNDER_RR")) {
    // Printing timestamps requires an IPC to get the local time, which
    // does not work in an rr replay session. Just disable timestamp printing,
    // which we don't care about anyway.
    logging::SetLogItems(true, true, false, false);
  }

  StringBuilder string_blockflow;
  DumpLayoutObject(string_blockflow, true, kShowTreeCharacterOffset);
  for (const RootInlineBox* root = FirstRootBox(); root;
       root = root->NextRootBox()) {
    root->DumpLineTreeAndMark(string_blockflow, marked_box1, marked_label1,
                              marked_box2, marked_label2, obj, 1);
  }
  DLOG(INFO) << "\n" << string_blockflow.ToString().Utf8();
}

#endif

void LayoutBlockFlow::AddOutlineRects(
    Vector<PhysicalRect>& rects,
    OutlineInfo* info,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  NOT_DESTROYED();

  LayoutBlock::AddOutlineRects(rects, info, additional_offset,
                               include_block_overflows);

  if (ShouldIncludeBlockVisualOverflow(include_block_overflows) &&
      !HasNonVisibleOverflow() && !HasControlClip()) {
    for (RootInlineBox* curr = FirstRootBox(); curr;
         curr = curr->NextRootBox()) {
      LayoutUnit flipped_left = curr->X();
      LayoutUnit flipped_right = curr->X() + curr->Width();
      LayoutUnit top = curr->Y();
      LayoutUnit bottom = curr->Y() + curr->Height();
      if (IsHorizontalWritingMode()) {
        top = std::max(curr->LineTop(), top);
        bottom = std::min(curr->LineBottom(), bottom);
      } else {
        flipped_left = std::max(curr->LineTop(), flipped_left);
        flipped_right = std::min(curr->LineBottom(), flipped_right);
      }
      LayoutRect rect(flipped_left, top, flipped_right - flipped_left,
                      bottom - top);
      if (!rect.IsEmpty()) {
        PhysicalRect physical_rect = FlipForWritingMode(rect);
        physical_rect.Move(additional_offset);
        rects.push_back(physical_rect);
      }
    }
  }
}

void LayoutBlockFlow::InvalidateDisplayItemClients(
    PaintInvalidationReason invalidation_reason) const {
  NOT_DESTROYED();
  BlockFlowPaintInvalidator(*this).InvalidateDisplayItemClients(
      invalidation_reason);
}

LayoutBlockFlow::LayoutBlockFlowRareData::LayoutBlockFlowRareData(
    const LayoutBlockFlow* block)
    : break_before_(static_cast<unsigned>(EBreakBetween::kAuto)),
      break_after_(static_cast<unsigned>(EBreakBetween::kAuto)),
      line_break_to_avoid_widow_(-1),
      did_break_at_line_to_avoid_widow_(false) {}

LayoutBlockFlow::LayoutBlockFlowRareData::~LayoutBlockFlowRareData() = default;

void LayoutBlockFlow::LayoutBlockFlowRareData::Trace(Visitor* visitor) const {
  visitor->Trace(multi_column_flow_thread_);
  visitor->Trace(offset_mapping_);
}

void LayoutBlockFlow::ClearOffsetMappingIfNeeded() {
  NOT_DESTROYED();
  DCHECK(!IsLayoutNGObject());
  if (!rare_data_)
    return;
  rare_data_->offset_mapping_.Clear();
}

const NGOffsetMapping* LayoutBlockFlow::GetOffsetMapping() const {
  NOT_DESTROYED();
  DCHECK(!IsLayoutNGObject());
  CHECK(!SelfNeedsLayout());
  CHECK(!NeedsLayout() || ChildLayoutBlockedByDisplayLock());
  return rare_data_ ? rare_data_->offset_mapping_ : nullptr;
}

void LayoutBlockFlow::SetOffsetMapping(NGOffsetMapping* offset_mapping) {
  NOT_DESTROYED();
  DCHECK(!IsLayoutNGObject());
  DCHECK(offset_mapping);
  EnsureRareData().offset_mapping_ = offset_mapping;
}

}  // namespace blink
