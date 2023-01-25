/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"
#include "third_party/blink/renderer/core/paint/multi_column_set_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

LayoutMultiColumnSet::LayoutMultiColumnSet(LayoutFlowThread* flow_thread)
    : LayoutBlockFlow(nullptr),
      fragmentainer_groups_(*this),
      flow_thread_(flow_thread),
      initial_height_calculated_(false),
      last_actual_column_count_(0) {}

LayoutMultiColumnSet* LayoutMultiColumnSet::CreateAnonymous(
    LayoutFlowThread& flow_thread,
    const ComputedStyle& parent_style) {
  Document& document = flow_thread.GetDocument();
  LayoutMultiColumnSet* layout_object =
      MakeGarbageCollected<LayoutMultiColumnSet>(&flow_thread);
  layout_object->SetDocumentForAnonymous(&document);
  layout_object->SetStyle(
      document.GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent_style, EDisplay::kBlock));
  return layout_object;
}

void LayoutMultiColumnSet::Trace(Visitor* visitor) const {
  visitor->Trace(fragmentainer_groups_);
  visitor->Trace(flow_thread_);
  LayoutBlockFlow::Trace(visitor);
}

unsigned LayoutMultiColumnSet::FragmentainerGroupIndexAtFlowThreadOffset(
    LayoutUnit flow_thread_offset,
    PageBoundaryRule rule) const {
  NOT_DESTROYED();
  DCHECK_GT(fragmentainer_groups_.size(), 0u);
  if (flow_thread_offset <= 0)
    return 0;
  for (unsigned index = 0; index < fragmentainer_groups_.size(); index++) {
    const auto& row = fragmentainer_groups_[index];
    if (rule == kAssociateWithLatterPage) {
      if (row.LogicalTopInFlowThread() <= flow_thread_offset &&
          row.LogicalBottomInFlowThread() > flow_thread_offset)
        return index;
    } else if (row.LogicalTopInFlowThread() < flow_thread_offset &&
               row.LogicalBottomInFlowThread() >= flow_thread_offset) {
      return index;
    }
  }
  return fragmentainer_groups_.size() - 1;
}

const MultiColumnFragmentainerGroup&
LayoutMultiColumnSet::FragmentainerGroupAtVisualPoint(
    const LayoutPoint& visual_point) const {
  NOT_DESTROYED();
  DCHECK_GT(fragmentainer_groups_.size(), 0u);
  LayoutUnit block_offset =
      IsHorizontalWritingMode() ? visual_point.Y() : visual_point.X();
  for (unsigned index = 0; index < fragmentainer_groups_.size(); index++) {
    const auto& row = fragmentainer_groups_[index];
    if (row.LogicalTop() + row.GroupLogicalHeight() > block_offset)
      return row;
  }
  return fragmentainer_groups_.Last();
}

LayoutUnit LayoutMultiColumnSet::PageLogicalHeightForOffset(
    LayoutUnit offset) const {
  NOT_DESTROYED();
  DCHECK(IsPageLogicalHeightKnown());
  const MultiColumnFragmentainerGroup& last_row = LastFragmentainerGroup();
  if (offset >= last_row.LogicalTopInFlowThread() +
                    FragmentainerGroupCapacity(last_row)) {
    // The offset is outside the bounds of the fragmentainer groups that we
    // have established at this point. If we're nested inside another
    // fragmentation context, and are allowed to create additional rows, we
    // need to calculate the height on our own.
    const LayoutMultiColumnFlowThread* flow_thread = MultiColumnFlowThread();
    FragmentationContext* enclosing_fragmentation_context =
        flow_thread->EnclosingFragmentationContext();
    if (enclosing_fragmentation_context &&
        NeedsNewFragmentainerGroupAt(offset, kAssociateWithLatterPage) &&
        enclosing_fragmentation_context->IsFragmentainerLogicalHeightKnown()) {
      // We'd ideally like to translate |offset| to an offset in the coordinate
      // space of the enclosing fragmentation context here, but that's hard,
      // since the offset is out of bounds. So just use the bottom we have
      // found so far.
      LayoutUnit enclosing_context_bottom =
          last_row.BlockOffsetInEnclosingFragmentationContext() +
          last_row.GroupLogicalHeight();
      LayoutUnit extra_row_height =
          enclosing_fragmentation_context->FragmentainerLogicalHeightAt(
              enclosing_context_bottom);
      LayoutUnit current_multicol_height = LogicalTopFromMulticolContentEdge() +
                                           last_row.LogicalTop() +
                                           last_row.GroupLogicalHeight();
      // Constrain against specified height / max-height.
      LayoutUnit multicol_height_with_extra_row =
          std::min(current_multicol_height + extra_row_height,
                   flow_thread->MaxColumnLogicalHeight());
      extra_row_height =
          multicol_height_with_extra_row - current_multicol_height;
      return extra_row_height.ClampNegativeToZero();
    }
  }
  return FragmentainerGroupAtFlowThreadOffset(offset, kAssociateWithLatterPage)
      .ColumnLogicalHeight();
}

LayoutUnit LayoutMultiColumnSet::PageRemainingLogicalHeightForOffset(
    LayoutUnit offset_in_flow_thread,
    PageBoundaryRule page_boundary_rule) const {
  NOT_DESTROYED();
  const MultiColumnFragmentainerGroup& row =
      FragmentainerGroupAtFlowThreadOffset(offset_in_flow_thread,
                                           page_boundary_rule);
  LayoutUnit page_logical_height = row.ColumnLogicalHeight();
  LayoutUnit page_logical_bottom =
      row.ColumnLogicalTopForOffset(offset_in_flow_thread) +
      page_logical_height;
  LayoutUnit remaining_logical_height =
      page_logical_bottom - offset_in_flow_thread;

  if (page_boundary_rule == kAssociateWithFormerPage) {
    // An offset exactly at a column boundary will act as being part of the
    // former column in question (i.e. no remaining space), rather than being
    // part of the latter (i.e. one whole column length of remaining space).
    remaining_logical_height =
        IntMod(remaining_logical_height, page_logical_height);
  } else if (!remaining_logical_height) {
    // When pageBoundaryRule is AssociateWithLatterPage, we shouldn't just
    // return 0 if there's no space left, because in that case we're at a
    // column boundary, in which case we should return the amount of space
    // remaining in the *next* column. Note that the page height itself may be
    // 0, though.
    remaining_logical_height = page_logical_height;
  }
  return remaining_logical_height;
}

bool LayoutMultiColumnSet::IsPageLogicalHeightKnown() const {
  NOT_DESTROYED();
  return FirstFragmentainerGroup().IsLogicalHeightKnown();
}

bool LayoutMultiColumnSet::NewFragmentainerGroupsAllowed() const {
  NOT_DESTROYED();
  if (!IsPageLogicalHeightKnown()) {
    // If we have no clue about the height of the multicol container, bail. This
    // situation occurs initially when an auto-height multicol container is
    // nested inside another auto-height multicol container. We need at least an
    // estimated height of the outer multicol container before we can check what
    // an inner fragmentainer group has room for.
    // Its height is indefinite for now.
    return false;
  }
  if (IsInitialHeightCalculated()) {
    // We only insert additional fragmentainer groups in the initial layout
    // pass. We only want to balance columns in the last fragmentainer group (if
    // we need to balance at all), so we want that last fragmentainer group to
    // be the same one in all layout passes that follow.
    return false;
  }
  return true;
}

LayoutUnit LayoutMultiColumnSet::NextLogicalTopForUnbreakableContent(
    LayoutUnit flow_thread_offset,
    LayoutUnit content_logical_height) const {
  NOT_DESTROYED();
  if (!MultiColumnFlowThread()->EnclosingFragmentationContext()) {
    // If there's no enclosing fragmentation context, there'll ever be only one
    // row, and all columns there will have the same height.
    return flow_thread_offset;
  }

  // Assert the problematic situation. If we have no problem with the column
  // height, why are we even here?
  DCHECK_LT(PageLogicalHeightForOffset(flow_thread_offset),
            content_logical_height);

  // There's a likelihood for subsequent rows to be taller than the first one.
  const MultiColumnFragmentainerGroup& first_row = FirstFragmentainerGroup();
  LayoutUnit first_row_logical_bottom_in_flow_thread =
      first_row.LogicalTopInFlowThread() +
      FragmentainerGroupCapacity(first_row);
  if (flow_thread_offset >= first_row_logical_bottom_in_flow_thread)
    return flow_thread_offset;  // We're not in the first row. Give up.
  LayoutUnit new_logical_height =
      PageLogicalHeightForOffset(first_row_logical_bottom_in_flow_thread);
  if (content_logical_height > new_logical_height) {
    // The next outer column or page doesn't have enough space either. Give up
    // and stay where we are.
    return flow_thread_offset;
  }
  return first_row_logical_bottom_in_flow_thread;
}

LayoutMultiColumnSet* LayoutMultiColumnSet::NextSiblingMultiColumnSet() const {
  NOT_DESTROYED();
  for (LayoutObject* sibling = NextSibling(); sibling;
       sibling = sibling->NextSibling()) {
    if (sibling->IsLayoutMultiColumnSet())
      return To<LayoutMultiColumnSet>(sibling);
  }
  return nullptr;
}

LayoutMultiColumnSet* LayoutMultiColumnSet::PreviousSiblingMultiColumnSet()
    const {
  NOT_DESTROYED();
  for (LayoutObject* sibling = PreviousSibling(); sibling;
       sibling = sibling->PreviousSibling()) {
    if (sibling->IsLayoutMultiColumnSet())
      return To<LayoutMultiColumnSet>(sibling);
  }
  return nullptr;
}

bool LayoutMultiColumnSet::NeedsNewFragmentainerGroupAt(
    LayoutUnit offset_in_flow_thread,
    PageBoundaryRule page_boundary_rule) const {
  NOT_DESTROYED();
  // First the cheap check: Perhaps the last fragmentainer group has sufficient
  // capacity?
  const MultiColumnFragmentainerGroup& last_row = LastFragmentainerGroup();
  LayoutUnit max_logical_bottom_in_flow_thread =
      last_row.LogicalTopInFlowThread() + FragmentainerGroupCapacity(last_row);
  if (page_boundary_rule == kAssociateWithFormerPage) {
    if (offset_in_flow_thread <= max_logical_bottom_in_flow_thread)
      return false;
  } else if (offset_in_flow_thread < max_logical_bottom_in_flow_thread) {
    return false;
  }

  // So, there's not enough room in the last fragmentainer group. However,
  // there can only ever be one fragmentainer group per column set if we're not
  // nested inside another fragmentation context. We'll just create overflowing
  // columns if the fragmentainer group cannot hold all the content.
  if (!MultiColumnFlowThread()->EnclosingFragmentationContext())
    return false;

  // If we have reached the limits of what a LayoutUnit can hold, we better
  // stop, or we'd end up with zero-height columns.
  if (offset_in_flow_thread.MightBeSaturated())
    return false;

  // We're in a nested fragmentation context, and the last fragmentainer group
  // cannot hold content at the specified offset without overflowing. This
  // usually warrants a new fragmentainer group; however, this will not be the
  // case if we have already allocated all available block space in this
  // multicol container. When setting up a new fragmentainer group, we always
  // constrain against the remaining portion of any specified
  // height/max-height. This means that we shouldn't allow creation of
  // fragmentainer groups below the bottom of the multicol container, or we'd
  // end up with zero-height fragmentainer groups (or actually 1px; see
  // heightAdjustedForRowOffset() in MultiColumnFragmentainerGroup, which
  // guards against zero-height groups), i.e. potentially a lot of pretty
  // useless fragmentainer groups, and possibly broken layout too. Instead,
  // we'll just allow additional (overflowing) columns to be created in the
  // last fragmentainer group, similar to what we do when we're not nested.
  LayoutUnit logical_bottom =
      last_row.LogicalTop() + last_row.GroupLogicalHeight();
  LayoutUnit space_used = logical_bottom + LogicalTopFromMulticolContentEdge();
  LayoutUnit max_column_height =
      MultiColumnFlowThread()->MaxColumnLogicalHeight();
  return max_column_height - space_used > LayoutUnit();
}

MultiColumnFragmentainerGroup&
LayoutMultiColumnSet::AppendNewFragmentainerGroup() {
  NOT_DESTROYED();
  MultiColumnFragmentainerGroup new_group(*this);
  {  // Extra scope here for previousGroup; it's potentially invalid once we
     // modify the m_fragmentainerGroups Vector.
    MultiColumnFragmentainerGroup& previous_group =
        fragmentainer_groups_.Last();

    // This is the flow thread block offset where |previousGroup| ends and
    // |newGroup| takes over.
    LayoutUnit block_offset_in_flow_thread =
        previous_group.LogicalTopInFlowThread() +
        FragmentainerGroupCapacity(previous_group);
    previous_group.SetLogicalBottomInFlowThread(block_offset_in_flow_thread);
    new_group.SetLogicalTopInFlowThread(block_offset_in_flow_thread);
    new_group.SetLogicalTop(previous_group.LogicalTop() +
                            previous_group.GroupLogicalHeight());
    new_group.ResetColumnHeight();
  }
  fragmentainer_groups_.Append(new_group);
  return fragmentainer_groups_.Last();
}

LayoutUnit LayoutMultiColumnSet::LogicalTopFromMulticolContentEdge() const {
  NOT_DESTROYED();
  // We subtract the position of the first column set or spanner placeholder,
  // rather than the "before" border+padding of the multicol container. This
  // distinction doesn't matter after layout, but during layout it does:
  // The flow thread (i.e. the multicol contents) is laid out before the column
  // sets and spanner placeholders, which means that compesating for a top
  // border+padding that hasn't yet been baked into the offset will produce the
  // wrong results in the first layout pass, and we'd end up performing a wasted
  // layout pass in many cases.
  const LayoutBox& first_column_box =
      *MultiColumnFlowThread()->FirstMultiColumnBox();
  // The top margin edge of the first column set or spanner placeholder is flush
  // with the top content edge of the multicol container. The margin here never
  // collapses with other margins, so we can just subtract it. Column sets never
  // have margins, but spanner placeholders may.
  LayoutUnit first_column_box_margin_edge =
      first_column_box.LogicalTop() -
      MultiColumnBlockFlow()->MarginBeforeForChild(first_column_box);
  return LogicalTop() - first_column_box_margin_edge;
}

LayoutUnit LayoutMultiColumnSet::LogicalTopInFlowThread() const {
  NOT_DESTROYED();
  return FirstFragmentainerGroup().LogicalTopInFlowThread();
}

LayoutUnit LayoutMultiColumnSet::LogicalBottomInFlowThread() const {
  NOT_DESTROYED();
  return LastFragmentainerGroup().LogicalBottomInFlowThread();
}

bool LayoutMultiColumnSet::HeightIsAuto() const {
  NOT_DESTROYED();
  LayoutMultiColumnFlowThread* flow_thread = MultiColumnFlowThread();
  // If support for the column-fill property isn't enabled, we want to behave
  // as if column-fill were auto, so that multicol containers with specified
  // height don't get their columns balanced (auto-height multicol containers
  // will still get their columns balanced, even if column-fill isn't 'balance'
  // - in accordance with the spec).
  // Pretending that column-fill is auto also matches the old multicol
  // implementation, which has no support for this property.
  if (MultiColumnBlockFlow()->StyleRef().GetColumnFill() ==
      EColumnFill::kBalance)
    return true;
  if (LayoutBox* next = NextSiblingBox()) {
    if (next->IsLayoutMultiColumnSpannerPlaceholder()) {
      // If we're followed by a spanner, we need to balance.
      return true;
    }
  }
  return !flow_thread->ColumnHeightAvailable();
}

LayoutSize LayoutMultiColumnSet::FlowThreadTranslationAtOffset(
    LayoutUnit block_offset,
    PageBoundaryRule rule,
    CoordinateSpaceConversion mode) const {
  NOT_DESTROYED();
  return FragmentainerGroupAtFlowThreadOffset(block_offset, rule)
      .FlowThreadTranslationAtOffset(block_offset, rule, mode);
}

LayoutPoint LayoutMultiColumnSet::VisualPointToFlowThreadPoint(
    const LayoutPoint& visual_point) const {
  NOT_DESTROYED();
  const MultiColumnFragmentainerGroup& row =
      FragmentainerGroupAtVisualPoint(visual_point);
  return row.VisualPointToFlowThreadPoint(visual_point -
                                          row.OffsetFromColumnSet());
}

LayoutUnit LayoutMultiColumnSet::PageLogicalTopForOffset(
    LayoutUnit offset) const {
  NOT_DESTROYED();
  return FragmentainerGroupAtFlowThreadOffset(offset, kAssociateWithLatterPage)
      .ColumnLogicalTopForOffset(offset);
}

bool LayoutMultiColumnSet::RecalculateColumnHeight() {
  NOT_DESTROYED();
  if (old_logical_top_ != LogicalTop() &&
      MultiColumnFlowThread()->EnclosingFragmentationContext()) {
    // Preceding spanners or column sets have been moved or resized. This means
    // that the fragmentainer groups that we have inserted need to be
    // re-inserted. Restart column balancing.
    ResetColumnHeight();
    return true;
  }

  bool changed = false;
  for (auto& group : fragmentainer_groups_)
    changed = group.RecalculateColumnHeight(*this) || changed;
  initial_height_calculated_ = true;
  return changed;
}

void LayoutMultiColumnSet::ResetColumnHeight() {
  NOT_DESTROYED();
  fragmentainer_groups_.DeleteExtraGroups();
  fragmentainer_groups_.First().ResetColumnHeight();
  tallest_unbreakable_logical_height_ = LayoutUnit();
  initial_height_calculated_ = false;
}

void LayoutMultiColumnSet::BeginFlow(LayoutUnit offset_in_flow_thread) {
  NOT_DESTROYED();
  // At this point layout is exactly at the beginning of this set. Store block
  // offset from flow thread start.
  fragmentainer_groups_.First().SetLogicalTopInFlowThread(
      offset_in_flow_thread);
}

void LayoutMultiColumnSet::EndFlow(LayoutUnit offset_in_flow_thread) {
  NOT_DESTROYED();
  // At this point layout is exactly at the end of this set. Store block offset
  // from flow thread start. This set is now considered "flowed", although we
  // may have to revisit it later (with beginFlow()), e.g. if a subtree in the
  // flow thread has to be laid out over again because the initial margin
  // collapsing estimates were wrong.
  fragmentainer_groups_.Last().SetLogicalBottomInFlowThread(
      offset_in_flow_thread);
}

void LayoutMultiColumnSet::StyleDidChange(StyleDifference diff,
                                          const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlockFlow::StyleDidChange(diff, old_style);

  // column-rule is specified on the parent (the multicol container) of this
  // object, but it's the column sets that are in charge of painting them.
  // A column rule is pretty much like any other box decoration, like borders.
  // We need to say that we have box decorations here, so that the columnn set
  // is invalidated when it gets laid out. We cannot check here whether the
  // multicol container actually has a visible column rule or not, because we
  // may not have been inserted into the tree yet. Painting a column set is
  // cheap anyway, because the only thing it can paint is the column rule, while
  // actual multicol content is handled by the flow thread.
  SetHasBoxDecorationBackground(true);
}

void LayoutMultiColumnSet::UpdateLayout() {
  NOT_DESTROYED();
  if (RecalculateColumnHeight())
    MultiColumnFlowThread()->SetColumnHeightsChanged();
  LayoutBlockFlow::UpdateLayout();

  auto actual_column_count = ActualColumnCount();
  if (actual_column_count != last_actual_column_count_) {
    // At least we need to paint column rules differently when actual column
    // count changes.
    SetShouldDoFullPaintInvalidation();
    last_actual_column_count_ = actual_column_count;
  }
}

MinMaxSizes LayoutMultiColumnSet::ComputeIntrinsicLogicalWidths() const {
  NOT_DESTROYED();
  return MinMaxSizes();
}

void LayoutMultiColumnSet::ComputeLogicalHeight(
    LayoutUnit,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  NOT_DESTROYED();
  LayoutUnit logical_height;
  // Under some circumstances column heights are unknown at this point. This
  // happens e.g. when this column set got pushed down by a preceding spanner
  // (and when that affects the available space for this column set). Just set
  // the height to 0 for now. Another layout pass has already been scheduled, to
  // calculate the correct height.
  if (IsPageLogicalHeightKnown()) {
    for (const auto& group : fragmentainer_groups_)
      logical_height += group.GroupLogicalHeight();
  }
  computed_values.extent_ = logical_height;
  computed_values.position_ = logical_top;
}

PositionWithAffinity LayoutMultiColumnSet::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  LayoutPoint flipped_point = FlipForWritingMode(point);
  // Convert the visual point to a flow thread point.
  const MultiColumnFragmentainerGroup& row =
      FragmentainerGroupAtVisualPoint(flipped_point);
  LayoutPoint flow_thread_point = row.VisualPointToFlowThreadPoint(
      flipped_point + row.OffsetFromColumnSet(),
      MultiColumnFragmentainerGroup::kSnapToColumn);
  // Then drill into the flow thread, where we'll find the actual content.
  return FlowThread()->PositionForPoint(
      FlowThread()->FlipForWritingMode(flow_thread_point));
}

LayoutUnit LayoutMultiColumnSet::ColumnGap() const {
  NOT_DESTROYED();
  LayoutBlockFlow* parent_block = MultiColumnBlockFlow();

  if (const absl::optional<Length>& column_gap =
          parent_block->StyleRef().ColumnGap())
    return ValueForLength(*column_gap, AvailableLogicalWidth());

  // "1em" is recommended as the normal gap setting. Matches <p> margins.
  return LayoutUnit(
      parent_block->StyleRef().GetFontDescription().ComputedPixelSize());
}

unsigned LayoutMultiColumnSet::ActualColumnCount() const {
  NOT_DESTROYED();
  // FIXME: remove this method. It's a meaningless question to ask the set "how
  // many columns do you actually have?", since that may vary for each row.
  return FirstFragmentainerGroup().ActualColumnCount();
}

void LayoutMultiColumnSet::PaintObject(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  MultiColumnSetPainter(*this).PaintObject(paint_info, paint_offset);
}

LayoutRect LayoutMultiColumnSet::FragmentsBoundingBox(
    const LayoutRect& bounding_box_in_flow_thread) const {
  NOT_DESTROYED();
  LayoutRect result;
  for (const auto& group : fragmentainer_groups_)
    result.Unite(group.FragmentsBoundingBox(bounding_box_in_flow_thread));
  return result;
}

void LayoutMultiColumnSet::ComputeVisualOverflow(bool recompute_floats) {
  NOT_DESTROYED();
  LayoutRect previous_visual_overflow_rect = VisualOverflowRectAllowingUnset();
  ClearVisualOverflow();
  AddVisualOverflowFromChildren();
  AddVisualEffectOverflow();

  if (recompute_floats || CreatesNewFormattingContext() ||
      HasSelfPaintingLayer())
    AddVisualOverflowFromFloats();

  if (VisualOverflowRect() != previous_visual_overflow_rect) {
    InvalidateIntersectionObserverCachedRects();
    SetShouldCheckForPaintInvalidation();
    GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);
  }
}

void LayoutMultiColumnSet::AddVisualOverflowFromChildren() {
  NOT_DESTROYED();
  if (ChildLayoutBlockedByDisplayLock())
    return;

  // It's useless to calculate overflow if we haven't determined the page
  // logical height yet.
  if (!IsPageLogicalHeightKnown())
    return;
  LayoutRect overflow_rect;
  for (const auto& group : fragmentainer_groups_) {
    LayoutRect rect = group.CalculateOverflow();
    rect.Move(group.OffsetFromColumnSet());
    overflow_rect.Unite(rect);
  }
  AddContentsVisualOverflow(overflow_rect);
}

void LayoutMultiColumnSet::AddLayoutOverflowFromChildren() {
  NOT_DESTROYED();
  if (ChildLayoutBlockedByDisplayLock())
    return;

  // It's useless to calculate overflow if we haven't determined the page
  // logical height yet.
  if (!IsPageLogicalHeightKnown())
    return;
  LayoutRect overflow_rect;
  for (const auto& group : fragmentainer_groups_) {
    LayoutRect rect = group.CalculateOverflow();
    rect.Move(group.OffsetFromColumnSet());
    overflow_rect.Unite(rect);
  }
  AddLayoutOverflow(overflow_rect);
}

void LayoutMultiColumnSet::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBlockFlow::InsertedIntoTree();
  AttachToFlowThread();
}

void LayoutMultiColumnSet::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutBlockFlow::WillBeRemovedFromTree();
  DetachFromFlowThread();
}

LayoutSize LayoutMultiColumnSet::Size() const {
  NOT_DESTROYED();
  // TODO(crbug.com/1353190): Do not refer to frame_size_.
  return frame_size_;
}

void LayoutMultiColumnSet::AttachToFlowThread() {
  NOT_DESTROYED();
  if (DocumentBeingDestroyed())
    return;

  if (!flow_thread_)
    return;

  flow_thread_->AddColumnSetToThread(this);
}

void LayoutMultiColumnSet::DetachFromFlowThread() {
  NOT_DESTROYED();
  if (flow_thread_) {
    flow_thread_->RemoveColumnSetFromThread(this);
    flow_thread_ = nullptr;
  }
}

LayoutRect LayoutMultiColumnSet::FlowThreadPortionRect() const {
  NOT_DESTROYED();
  LayoutRect portion_rect(LayoutUnit(), LogicalTopInFlowThread(),
                          PageLogicalWidth(), LogicalHeightInFlowThread());
  if (!IsHorizontalWritingMode())
    return portion_rect.TransposedRect();
  return portion_rect;
}

bool LayoutMultiColumnSet::ComputeColumnRuleBounds(
    const LayoutPoint& paint_offset,
    Vector<LayoutRect>& column_rule_bounds) const {
  NOT_DESTROYED();
  // Reference: https://www.w3.org/TR/css3-multicol/#column-gaps-and-rules
  const ComputedStyle& block_style = MultiColumnBlockFlow()->StyleRef();
  bool rule_transparent = block_style.ColumnRuleIsTransparent();
  EBorderStyle rule_style = block_style.ColumnRuleStyle();
  LayoutUnit rule_thickness(block_style.ColumnRuleWidth());
  LayoutUnit col_gap = ColumnGap();
  bool render_rule =
      ComputedStyle::BorderStyleIsVisible(rule_style) && !rule_transparent;
  if (!render_rule)
    return false;

  unsigned col_count = ActualColumnCount();
  if (col_count <= 1)
    return false;

  bool left_to_right = StyleRef().IsLeftToRightDirection();
  LayoutUnit curr_logical_left_offset =
      left_to_right ? LayoutUnit() : ContentLogicalWidth();
  LayoutUnit rule_add = BorderAndPaddingLogicalLeft();
  LayoutUnit rule_logical_left =
      left_to_right ? LayoutUnit() : ContentLogicalWidth();
  LayoutUnit inline_direction_size = PageLogicalWidth();

  for (unsigned i = 0; i < col_count; i++) {
    // Move to the next position.
    if (left_to_right) {
      rule_logical_left += inline_direction_size + col_gap / 2;
      curr_logical_left_offset += inline_direction_size + col_gap;
    } else {
      rule_logical_left -= (inline_direction_size + col_gap / 2);
      curr_logical_left_offset -= (inline_direction_size + col_gap);
    }

    // Now compute the final bounds.
    if (i < col_count - 1) {
      LayoutUnit rule_left, rule_right, rule_top, rule_bottom;
      if (IsHorizontalWritingMode()) {
        rule_left = paint_offset.X() + rule_logical_left - rule_thickness / 2 +
                    rule_add;
        rule_right = rule_left + rule_thickness;
        rule_top = paint_offset.Y() + BorderTop() + PaddingTop();
        rule_bottom = rule_top + ContentHeight();
      } else {
        rule_left = paint_offset.X() + BorderLeft() + PaddingLeft();
        rule_right = rule_left + ContentWidth();
        rule_top = paint_offset.Y() + rule_logical_left - rule_thickness / 2 +
                   rule_add;
        rule_bottom = rule_top + rule_thickness;
      }

      column_rule_bounds.push_back(LayoutRect(
          rule_left, rule_top, rule_right - rule_left, rule_bottom - rule_top));
    }

    rule_logical_left = curr_logical_left_offset;
  }
  return true;
}

PhysicalRect LayoutMultiColumnSet::LocalVisualRectIgnoringVisibility() const {
  NOT_DESTROYED();
  PhysicalRect block_flow_bounds =
      LayoutBlockFlow::LocalVisualRectIgnoringVisibility();

  // Now add in column rule bounds, if present.
  Vector<LayoutRect> column_rule_bounds;
  if (ComputeColumnRuleBounds(LayoutPoint(), column_rule_bounds)) {
    block_flow_bounds.Unite(
        PhysicalRectToBeNoop(UnionRect(column_rule_bounds)));
  }

  return block_flow_bounds;
}

void LayoutMultiColumnSet::FinishLayoutFromNG() {
  NOT_DESTROYED();
  // Calculate the block-size of all the fragmentainer groups combined.
  LogicalExtentComputedValues computed_values;
  ComputeLogicalHeight(/* logical_height */ LayoutUnit(),
                       /* logical_top */ LayoutUnit(), computed_values);
  SetLogicalHeight(computed_values.extent_);
  ComputeLayoutOverflow(computed_values.extent_);
}

void LayoutMultiColumnSet::SetIsIgnoredByNG() {
  NOT_DESTROYED();
  fragmentainer_groups_.First().SetColumnBlockSizeFromNG(LayoutUnit());
}

}  // namespace blink
