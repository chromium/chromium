// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/column_balancer.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"

namespace blink {

ColumnBalancer::ColumnBalancer(const LayoutMultiColumnSet& column_set,
                               LayoutUnit logical_top_in_flow_thread,
                               LayoutUnit logical_bottom_in_flow_thread)
    : column_set_(column_set),
      logical_top_in_flow_thread_(logical_top_in_flow_thread),
      logical_bottom_in_flow_thread_(logical_bottom_in_flow_thread) {
  DCHECK_GE(column_set.UsedColumnCount(), 1U);
}

void ColumnBalancer::Traverse() {
  TraverseSubtree(*ColumnSet().FlowThread());
  DCHECK(!FlowThreadOffset());
}

void ColumnBalancer::TraverseSubtree(const LayoutBox& box) {
  auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(box);
  if (box.ChildrenInline() && layout_block_flow) {
    // Look for breaks between lines.
    TraverseLines(*layout_block_flow);
  }

  // Look for breaks between and inside block-level children. Even if this is a
  // block flow with inline children, there may be interesting floats to examine
  // here.
  TraverseChildren(box);
}

void ColumnBalancer::TraverseLines(const LayoutBlockFlow& block_flow) {
  for (const RootInlineBox* line = block_flow.FirstRootBox(); line;
       line = line->NextRootBox()) {
    LayoutUnit line_top_in_flow_thread =
        flow_thread_offset_ + line->LineTopWithLeading();
    if (line_top_in_flow_thread < LogicalTopInFlowThread()) {
      // If the line is fully about the flow thread portion range we're working
      // with, we can skip it. If its logical top is outside the range, but its
      // logical bottom protrudes into the range, we need to examine it.
      LayoutUnit line_bottom = line->LineBottomWithLeading();
      if (flow_thread_offset_ + line_bottom <= LogicalTopInFlowThread())
        continue;
    }
    if (line_top_in_flow_thread >= LogicalBottomInFlowThread())
      break;
    ExamineLine(*line);
  }
}

void ColumnBalancer::TraverseChildren(const LayoutObject& object) {
  // The break-after value from the previous in-flow block-level object to be
  // joined with the break-before value of the next in-flow block-level sibling.
  EBreakBetween previous_break_after_value = EBreakBetween::kAuto;

  for (const LayoutObject* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsBox()) {
      // Keep traversing inside inlines. There may be floats there.
      if (child->IsLayoutInline())
        TraverseChildren(*child);
      continue;
    }

    const LayoutBox& child_box = ToLayoutBox(*child);

    LayoutUnit border_edge_offset;
    LayoutUnit logical_top = child_box.LogicalTop();
    LayoutUnit logical_height = child_box.LogicalHeightWithVisibleOverflow();
    // Floats' margins don't collapse with column boundaries, and we don't want
    // to break inside them, or separate them from the float's border box. Set
    // the offset to the margin-before edge (rather than border-before edge),
    // and include the block direction margins in the child height.
    if (child_box.IsFloating()) {
      LayoutUnit margin_before = child_box.MarginBefore(object.Style());
      LayoutUnit margin_after = child_box.MarginAfter(object.Style());
      logical_height =
          std::max(logical_height, child_box.LogicalHeight() + margin_after);
      logical_top -= margin_before;
      logical_height += margin_before;

      // As soon as we want to process content inside this child, though, we
      // need to get to its border-before edge.
      border_edge_offset = margin_before;
    }

    if (flow_thread_offset_ + logical_top + logical_height <=
        LogicalTopInFlowThread()) {
      // This child is fully above the flow thread portion we're examining.
      continue;
    }
    if (flow_thread_offset_ + logical_top >= LogicalBottomInFlowThread()) {
      // This child is fully below the flow thread portion we're examining. We
      // cannot just stop here, though, thanks to negative margins.
      // So keep looking.
      continue;
    }
    if (child_box.IsOutOfFlowPositioned() || child_box.IsColumnSpanAll())
      continue;

    // Tables are wicked. Both table rows and table cells are relative to their
    // table section.
    LayoutUnit offset_for_this_child =
        child_box.IsTableRow() ? LayoutUnit() : logical_top;

    // Include this child's offset in the flow thread offset. Note that rather
    // than subtracting the offset again when done, we set it back to the old
    // value. This matters in saturated arithmetic situations.
    auto old_flow_thread_offset = flow_thread_offset_;
    flow_thread_offset_ += offset_for_this_child;

    ExamineBoxAfterEntering(child_box, logical_height,
                            previous_break_after_value);
    // Unless the child is unsplittable, or if the child establishes an inner
    // multicol container, we descend into its subtree for further examination.
    auto* chlid_block_flow = DynamicTo<LayoutBlockFlow>(child_box);
    if (child_box.GetPaginationBreakability() != LayoutBox::kForbidBreaks &&
        (!chlid_block_flow || !chlid_block_flow->MultiColumnFlowThread())) {
      // We need to get to the border edge before processing content inside
      // this child. If the child is floated, we're currently at the margin
      // edge.
      auto old_flow_thread_offset = flow_thread_offset_;
      flow_thread_offset_ += border_edge_offset;
      TraverseSubtree(child_box);
      flow_thread_offset_ = old_flow_thread_offset;
    }
    previous_break_after_value = child_box.BreakAfter();
    ExamineBoxBeforeLeaving(child_box, logical_height);

    flow_thread_offset_ = old_flow_thread_offset;
  }
}

InitialColumnHeightFinder::InitialColumnHeightFinder(
    const LayoutMultiColumnSet& column_set,
    LayoutUnit logical_top_in_flow_thread,
    LayoutUnit logical_bottom_in_flow_thread)
    : ColumnBalancer(column_set,
                     logical_top_in_flow_thread,
                     logical_bottom_in_flow_thread) {
  shortest_struts_.resize(column_set.UsedColumnCount());
  for (auto& strut : shortest_struts_)
    strut = LayoutUnit::Max();
  Traverse();
  // We have now found each explicit / forced break, and their location. Now we
  // need to figure out how many additional implicit / soft breaks we need and
  // guess where they will occur, in order
  // to provide an initial column height.
  DistributeImplicitBreaks();
}

LayoutUnit InitialColumnHeightFinder::InitialMinimalBalancedHeight() const {
  LayoutUnit row_logical_top;
  if (content_runs_.size() > ColumnSet().UsedColumnCount()) {
    // We have not inserted additional fragmentainer groups yet (because we
    // aren't able to calculate their constraints yet), but we already know for
    // sure that there'll be more than one of them, due to the number of forced
    // breaks in a nested multicol container. We will now attempt to take all
    // the imaginary rows into account and calculate a minimal balanced logical
    // height for everything.
    unsigned stride = ColumnSet().UsedColumnCount();
    LayoutUnit row_start_offset = LogicalTopInFlowThread();
    for (unsigned i = 0; i < FirstContentRunIndexInLastRow(); i += stride) {
      LayoutUnit row_end_offset = content_runs_[i + stride - 1].BreakOffset();
      float row_height =
          float(row_end_offset - row_start_offset) / float(stride);
      row_logical_top += LayoutUnit::FromFloatCeil(row_height);
      row_start_offset = row_end_offset;
    }
  }
  unsigned index = ContentRunIndexWithTallestColumns();
  LayoutUnit start_offset = index > 0 ? content_runs_[index - 1].BreakOffset()
                                      : LogicalTopInFlowThread();
  LayoutUnit height = content_runs_[index].ColumnLogicalHeight(start_offset);
  return row_logical_top +
         std::max(height, tallest_unbreakable_logical_height_);
}

void InitialColumnHeightFinder::ExamineBoxAfterEntering(
    const LayoutBox& box,
    LayoutUnit child_logical_height,
    EBreakBetween previous_break_after_value) {
  if (last_break_seen_ > FlowThreadOffset()) {
    // We have moved backwards. We're probably in a parallel flow, caused by
    // floats, sibling table cells, etc.
    last_break_seen_ = LayoutUnit();
  }
  if (IsLogicalTopWithinBounds(FlowThreadOffset() - box.PaginationStrut())) {
    if (box.NeedsForcedBreakBefore(previous_break_after_value)) {
      AddContentRun(FlowThreadOffset());
    } else if (IsFirstAfterBreak(FlowThreadOffset()) &&
               last_break_seen_ != FlowThreadOffset()) {
      // This box is first after a soft break.
      last_break_seen_ = FlowThreadOffset();
      RecordStrutBeforeOffset(FlowThreadOffset(), box.PaginationStrut());
    }
  }

  if (box.GetPaginationBreakability() != LayoutBox::kAllowAnyBreaks) {
    tallest_unbreakable_logical_height_ =
        std::max(tallest_unbreakable_logical_height_, child_logical_height);
    return;
  }
  // Need to examine inner multicol containers to find their tallest unbreakable
  // piece of content.
  auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(box);
  if (!layout_block_flow)
    return;
  LayoutMultiColumnFlowThread* inner_flow_thread =
      layout_block_flow->MultiColumnFlowThread();
  if (!inner_flow_thread)
    return;
  LayoutUnit offset_in_inner_flow_thread =
      FlowThreadOffset() -
      inner_flow_thread->BlockOffsetInEnclosingFragmentationContext();
  LayoutUnit inner_unbreakable_height =
      inner_flow_thread->TallestUnbreakableLogicalHeight(
          offset_in_inner_flow_thread);
  tallest_unbreakable_logical_height_ =
      std::max(tallest_unbreakable_logical_height_, inner_unbreakable_height);
}

void InitialColumnHeightFinder::ExamineBoxBeforeLeaving(
    const LayoutBox& box,
    LayoutUnit child_logical_height) {}

static inline LayoutUnit ColumnLogicalHeightRequirementForLine(
    const ComputedStyle& style,
    const RootInlineBox& last_line) {
  // We may require a certain minimum number of lines per page in order to
  // satisfy orphans and widows, and that may affect the minimum page height.
  unsigned minimum_line_count =
      std::max<unsigned>(style.Orphans(), style.Widows());
  const RootInlineBox* line = &last_line;
  LayoutUnit logical_height_requirement;
  for (unsigned i = 0; i < minimum_line_count && line; i++) {
    logical_height_requirement +=
        line->LineBottomWithLeading() - line->LineTopWithLeading();
    line = line->PrevRootBox();
  }
  return logical_height_requirement;
}

void InitialColumnHeightFinder::ExamineLine(const RootInlineBox& line) {
  LayoutUnit line_top = line.LineTopWithLeading();
  LayoutUnit line_top_in_flow_thread = FlowThreadOffset() + line_top;
  LayoutUnit minimum_logial_height =
      ColumnLogicalHeightRequirementForLine(line.Block().StyleRef(), line);
  if (line_top_in_flow_thread < LayoutUnit())
    minimum_logial_height += line_top_in_flow_thread;
  tallest_unbreakable_logical_height_ =
      std::max(tallest_unbreakable_logical_height_, minimum_logial_height);
  if (IsFirstAfterBreak(line_top_in_flow_thread) &&
      last_break_seen_ != line_top_in_flow_thread) {
    last_break_seen_ = line_top_in_flow_thread;
    RecordStrutBeforeOffset(line_top_in_flow_thread, line.PaginationStrut());
  }
}

void InitialColumnHeightFinder::RecordStrutBeforeOffset(
    LayoutUnit offset_in_flow_thread,
    LayoutUnit strut) {
  unsigned column_count = ColumnSet().UsedColumnCount();
  DCHECK_EQ(shortest_struts_.size(), column_count);
  unsigned index =
      GroupAtOffset(offset_in_flow_thread)
          .ColumnIndexAtOffset(offset_in_flow_thread - strut,
                               LayoutBox::kAssociateWithFormerPage);
  if (index >= column_count)
    return;
  shortest_struts_[index] = std::min(shortest_struts_[index], strut);
}

LayoutUnit InitialColumnHeightFinder::SpaceUsedByStrutsAt(
    LayoutUnit offset_in_flow_thread) const {
  unsigned stop_before_column =
      GroupAtOffset(offset_in_flow_thread)
          .ColumnIndexAtOffset(offset_in_flow_thread,
                               LayoutBox::kAssociateWithLatterPage) +
      1;
  stop_before_column =
      std::min(stop_before_column, ColumnSet().UsedColumnCount());
  DCHECK_LE(stop_before_column, shortest_struts_.size());
  LayoutUnit total_strut_space;
  for (unsigned i = 0; i < stop_before_column; i++) {
    if (shortest_struts_[i] != LayoutUnit::Max())
      total_strut_space += shortest_struts_[i];
  }
  return total_strut_space;
}

void InitialColumnHeightFinder::AddContentRun(
    LayoutUnit end_offset_in_flow_thread) {
  end_offset_in_flow_thread -= SpaceUsedByStrutsAt(end_offset_in_flow_thread);
  if (!content_runs_.IsEmpty() &&
      end_offset_in_flow_thread <= content_runs_.back().BreakOffset())
    return;
  // Append another item as long as we haven't exceeded used column count. What
  // ends up in the overflow area shouldn't affect column balancing. However, if
  // we're in a nested fragmentation context, we may still need to record all
  // runs, since there'll be no overflow area in the inline direction then, but
  // rather additional rows of columns in multiple outer fragmentainers.
  if (content_runs_.size() >= ColumnSet().UsedColumnCount()) {
    const auto* flow_thread = ColumnSet().MultiColumnFlowThread();
    if (!flow_thread->EnclosingFragmentationContext() ||
        ColumnSet().NewFragmentainerGroupsAllowed())
      return;
  }
  content_runs_.push_back(ContentRun(end_offset_in_flow_thread));
}

unsigned InitialColumnHeightFinder::ContentRunIndexWithTallestColumns() const {
  unsigned index_with_largest_height = 0;
  LayoutUnit largest_height;
  LayoutUnit previous_offset = LogicalTopInFlowThread();
  size_t run_count = content_runs_.size();
  DCHECK(run_count);
  for (size_t i = FirstContentRunIndexInLastRow(); i < run_count; i++) {
    const ContentRun& run = content_runs_[i];
    LayoutUnit height = run.ColumnLogicalHeight(previous_offset);
    if (largest_height < height) {
      largest_height = height;
      index_with_largest_height = i;
    }
    previous_offset = run.BreakOffset();
  }
  return index_with_largest_height;
}

void InitialColumnHeightFinder::DistributeImplicitBreaks() {
  // Insert a final content run to encompass all content. This will include
  // overflow if we're at the end of the multicol container.
  AddContentRun(LogicalBottomInFlowThread());
  unsigned column_count = content_runs_.size();

  // If there is room for more breaks (to reach the used value of column-count),
  // imagine that we insert implicit breaks at suitable locations. At any given
  // time, the content run with the currently tallest columns will get another
  // implicit break "inserted", which will increase its column count by one and
  // shrink its columns' height. Repeat until we have the desired total number
  // of breaks. The largest column height among the runs will then be the
  // initial column height for the balancer to use.
  if (column_count > ColumnSet().UsedColumnCount()) {
    // If we exceed used column-count (which we are allowed to do if we're at
    // the initial balancing pass for a multicol that lives inside another
    // to-be-balanced outer multicol container), we only care about content that
    // could end up in the last row. We need to pad up the number of columns, so
    // that all rows will contain as many columns as used column-count dictates.
    column_count %= ColumnSet().UsedColumnCount();
    // If there are just enough explicit breaks to fill all rows with the right
    // amount of columns, we won't be needing any implicit breaks.
    if (!column_count)
      return;
  }
  while (column_count < ColumnSet().UsedColumnCount()) {
    unsigned index = ContentRunIndexWithTallestColumns();
    content_runs_[index].AssumeAnotherImplicitBreak();
    column_count++;
  }
}

MinimumSpaceShortageFinder::MinimumSpaceShortageFinder(
    const LayoutMultiColumnSet& column_set,
    LayoutUnit logical_top_in_flow_thread,
    LayoutUnit logical_bottom_in_flow_thread)
    : ColumnBalancer(column_set,
                     logical_top_in_flow_thread,
                     logical_bottom_in_flow_thread),
      minimum_space_shortage_(LayoutUnit::Max()),
      pending_strut_(LayoutUnit::Min()),
      forced_breaks_count_(0) {
  Traverse();
}

void MinimumSpaceShortageFinder::ExamineBoxAfterEntering(
    const LayoutBox& box,
    LayoutUnit child_logical_height,
    EBreakBetween previous_break_after_value) {
  LayoutBox::PaginationBreakability breakability =
      box.GetPaginationBreakability();

  // Look for breaks before the child box.
  if (IsLogicalTopWithinBounds(FlowThreadOffset() - box.PaginationStrut())) {
    if (box.NeedsForcedBreakBefore(previous_break_after_value)) {
      forced_breaks_count_++;
    } else {
      if (IsFirstAfterBreak(FlowThreadOffset())) {
        // This box is first after a soft break.
        LayoutUnit strut = box.PaginationStrut();
        // Figure out how much more space we would need to prevent it from being
        // pushed to the next column.
        RecordSpaceShortage(child_logical_height - strut);
        if (breakability != LayoutBox::kForbidBreaks &&
            pending_strut_ == LayoutUnit::Min()) {
          // We now want to look for the first piece of unbreakable content
          // (e.g. a line or a block-displayed image) inside this block. That
          // ought to be a good candidate for minimum space shortage; a much
          // better one than reporting space shortage for the entire block
          // (which we'll also do (further down), in case we couldn't find
          // anything more suitable).
          pending_strut_ = strut;
        }
      }
    }
  }

  if (breakability != LayoutBox::kForbidBreaks) {
    // See if this breakable box crosses column boundaries.
    LayoutUnit bottom_in_flow_thread =
        FlowThreadOffset() + child_logical_height;
    const MultiColumnFragmentainerGroup& group =
        GroupAtOffset(FlowThreadOffset());
    if (IsFirstAfterBreak(FlowThreadOffset()) ||
        group.ColumnLogicalTopForOffset(FlowThreadOffset()) !=
            group.ColumnLogicalTopForOffset(bottom_in_flow_thread)) {
      // If the child crosses a column boundary, record space shortage, in case
      // nothing inside it has already done so. The column balancer needs to
      // know by how much it has to stretch the columns to make more content
      // fit. If no breaks are reported (but do occur), the balancer will have
      // no clue. Only measure the space after the last column boundary, in case
      // it crosses more than one.
      LayoutUnit space_used_in_last_column =
          bottom_in_flow_thread -
          group.ColumnLogicalTopForOffset(bottom_in_flow_thread);
      RecordSpaceShortage(space_used_in_last_column);
    }
  }

  // If this is an inner multicol container, look for space shortage inside it.
  auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(box);
  if (!layout_block_flow)
    return;
  LayoutMultiColumnFlowThread* flow_thread =
      layout_block_flow->MultiColumnFlowThread();
  if (!flow_thread)
    return;
  for (const LayoutMultiColumnSet* column_set =
           flow_thread->FirstMultiColumnSet();
       column_set; column_set = column_set->NextSiblingMultiColumnSet()) {
    // Establish an inner shortage finder for this column set in the inner
    // multicol container. We need to let it walk through all fragmentainer
    // groups in one go, or we'd miss the column boundaries between each
    // fragmentainer group. We need to record space shortage there too.
    MinimumSpaceShortageFinder inner_finder(
        *column_set, column_set->LogicalTopInFlowThread(),
        column_set->LogicalBottomInFlowThread());
    RecordSpaceShortage(inner_finder.MinimumSpaceShortage());
  }
}

void MinimumSpaceShortageFinder::ExamineBoxBeforeLeaving(
    const LayoutBox& box,
    LayoutUnit child_logical_height) {
  if (pending_strut_ == LayoutUnit::Min() ||
      box.GetPaginationBreakability() != LayoutBox::kForbidBreaks)
    return;

  // The previous break was before a breakable block. Here's the first piece of
  // unbreakable content after / inside that block. We want to record the
  // distance from the top of the column to the bottom of this box as space
  // shortage.
  LayoutUnit logical_offset_from_current_column =
      OffsetFromColumnLogicalTop(FlowThreadOffset());
  RecordSpaceShortage(logical_offset_from_current_column +
                      child_logical_height - pending_strut_);
  pending_strut_ = LayoutUnit::Min();
}

void MinimumSpaceShortageFinder::ExamineLine(const RootInlineBox& line) {
  LayoutUnit line_top = line.LineTopWithLeading();
  LayoutUnit line_top_in_flow_thread = FlowThreadOffset() + line_top;
  LayoutUnit line_height = line.LineBottomWithLeading() - line_top;
  if (pending_strut_ != LayoutUnit::Min()) {
    // The previous break was before a breakable block. Here's the first line
    // after / inside that block. We want to record the distance from the top of
    // the column to the bottom of this box as space shortage.
    LayoutUnit logical_offset_from_current_column =
        OffsetFromColumnLogicalTop(line_top_in_flow_thread);
    RecordSpaceShortage(logical_offset_from_current_column + line_height -
                        pending_strut_);
    pending_strut_ = LayoutUnit::Min();
    return;
  }
  if (IsFirstAfterBreak(line_top_in_flow_thread))
    RecordSpaceShortage(line_height - line.PaginationStrut());

  // Even if the line box itself fits fine inside a column, some content may
  // overflow the line box bottom (due to restrictive line-height, for
  // instance). We should check if some portion of said overflow ends up in the
  // next column. That counts as space shortage.
  const MultiColumnFragmentainerGroup& group =
      GroupAtOffset(line_top_in_flow_thread);
  LayoutUnit line_bottom_with_overflow =
      line_top_in_flow_thread + line.LineBottom() - line_top;
  if (group.ColumnLogicalTopForOffset(line_top_in_flow_thread) !=
      group.ColumnLogicalTopForOffset(line_bottom_with_overflow)) {
    LayoutUnit shortage =
        line_bottom_with_overflow -
        group.ColumnLogicalTopForOffset(line_bottom_with_overflow);
    RecordSpaceShortage(shortage);
  }
}

}  // namespace blink
