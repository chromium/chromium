// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"

#include "third_party/blink/renderer/core/layout/column_balancer.h"
#include "third_party/blink/renderer/core/layout/fragmentation_context.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"

namespace blink {

// Limit the maximum column count, to prevent potential performance problems.
static const unsigned kColumnCountClampMax = 10000;

// Clamp "infinite" clips to a number of pixels that can be losslessly
// converted to and from floating point, to avoid loss of precision.
// Note that tables have something similar, see
// TableLayoutAlgorithm::kTableMaxWidth.
static const int kMulticolMaxClipPixels = 1000000;

MultiColumnFragmentainerGroup::MultiColumnFragmentainerGroup(
    const LayoutMultiColumnSet& column_set)
    : column_set_(column_set) {}

bool MultiColumnFragmentainerGroup::IsFirstGroup() const {
  return &column_set_.FirstFragmentainerGroup() == this;
}

bool MultiColumnFragmentainerGroup::IsLastGroup() const {
  return &column_set_.LastFragmentainerGroup() == this;
}

LayoutSize MultiColumnFragmentainerGroup::OffsetFromColumnSet() const {
  LayoutSize offset(LayoutUnit(), LogicalTop());
  if (!column_set_.FlowThread()->IsHorizontalWritingMode())
    return offset.TransposedSize();
  return offset;
}

LayoutUnit
MultiColumnFragmentainerGroup::BlockOffsetInEnclosingFragmentationContext()
    const {
  return LogicalTop() + column_set_.LogicalTopFromMulticolContentEdge() +
         column_set_.MultiColumnFlowThread()
             ->BlockOffsetInEnclosingFragmentationContext();
}

LayoutUnit MultiColumnFragmentainerGroup::LogicalHeightInFlowThreadAt(
    unsigned column_index) const {
  DCHECK(IsLogicalHeightKnown());
  LayoutUnit column_height = ColumnLogicalHeight();
  LayoutUnit logical_top = LogicalTopInFlowThreadAt(column_index);
  LayoutUnit logical_bottom = logical_top + column_height;
  unsigned actual_count = ActualColumnCount();
  if (column_index + 1 >= actual_count) {
    // The last column may contain overflow content, if the actual column count
    // was clamped, so using the column height won't do. This is also a way to
    // stay within the bounds of the flow thread, if the last column happens to
    // contain LESS than the other columns. We also need this clamping if we're
    // given a column index *after* the last column. Height should obviously be
    // 0 then. We may be called with a column index that's one entry past the
    // end if we're dealing with zero-height content at the very end of the flow
    // thread, and this location is at a column boundary.
    if (column_index + 1 == actual_count)
      logical_bottom = LogicalBottomInFlowThread();
    else
      logical_bottom = logical_top;
  }
  return (logical_bottom - logical_top).ClampNegativeToZero();
}

void MultiColumnFragmentainerGroup::ResetColumnHeight() {
  max_logical_height_ = CalculateMaxColumnHeight();

  LayoutMultiColumnFlowThread* flow_thread =
      column_set_.MultiColumnFlowThread();
  if (column_set_.HeightIsAuto()) {
    FragmentationContext* enclosing_fragmentation_context =
        flow_thread->EnclosingFragmentationContext();
    if (enclosing_fragmentation_context &&
        enclosing_fragmentation_context->IsFragmentainerLogicalHeightKnown()) {
      // Set an initial height, based on the fragmentainer height in the outer
      // fragmentation context, in order to tell how much content this
      // MultiColumnFragmentainerGroup can hold, and when we need to append a
      // new one.
      is_logical_height_known_ = true;
      logical_height_ = max_logical_height_;
      return;
    }
  }
  // If the multicol container has a definite height, use it as the column
  // height. This even applies when we are to balance the columns. We'll still
  // use the definite height as an initial height, and lay out once at that
  // column height. If it turns out that the content needs less than this
  // height, we have to balance and shrink the height and lay out the columns
  // over again.
  if (LayoutUnit logical_height = flow_thread->ColumnHeightAvailable()) {
    is_logical_height_known_ = true;
    SetAndConstrainColumnHeight(HeightAdjustedForRowOffset(logical_height));
  } else {
    is_logical_height_known_ = false;
    logical_height_ = LayoutUnit();
  }
}

bool MultiColumnFragmentainerGroup::RecalculateColumnHeight(
    LayoutMultiColumnSet& column_set) {
  LayoutUnit old_column_height = logical_height_;

  max_logical_height_ = CalculateMaxColumnHeight();

  // Only the last row may have auto height, and thus be balanced. There are no
  // good reasons to balance the preceding rows, and that could potentially lead
  // to an insane number of layout passes as well.
  if (IsLastGroup() && column_set.HeightIsAuto()) {
    LayoutUnit new_column_height;
    if (!column_set.IsInitialHeightCalculated()) {
      // Initial balancing: Start with the lowest imaginable column height. Also
      // calculate the height of the tallest piece of unbreakable content.
      // Columns should never get any shorter than that (unless constrained by
      // max-height). Propagate this to our containing column set, in case there
      // is an outer multicol container that also needs to balance. After having
      // calculated the initial column height, the multicol container needs
      // another layout pass with the column height that we just calculated.
      InitialColumnHeightFinder initial_height_finder(
          column_set, LogicalTopInFlowThread(), LogicalBottomInFlowThread());
      column_set.PropagateTallestUnbreakableLogicalHeight(
          initial_height_finder.TallestUnbreakableLogicalHeight());
      new_column_height = initial_height_finder.InitialMinimalBalancedHeight();
    } else {
      // Rebalancing: After having laid out again, we'll need to rebalance if
      // the height wasn't enough and we're allowed to stretch it, and then
      // re-lay out.  There are further details on the column balancing
      // machinery in ColumnBalancer and its derivates.
      new_column_height = RebalanceColumnHeightIfNeeded();
    }
    SetAndConstrainColumnHeight(new_column_height);
  } else {
    // The position of the column set may have changed, in which case height
    // available for columns may have changed as well.
    SetAndConstrainColumnHeight(logical_height_);
  }

  // We may not have found our final height yet, but at least we've found a
  // height.
  is_logical_height_known_ = true;

  if (logical_height_ == old_column_height)
    return false;  // No change. We're done.

  return true;  // Need another pass.
}

LayoutSize MultiColumnFragmentainerGroup::FlowThreadTranslationAtOffset(
    LayoutUnit offset_in_flow_thread,
    LayoutBox::PageBoundaryRule rule,
    CoordinateSpaceConversion mode) const {
  LayoutMultiColumnFlowThread* flow_thread =
      column_set_.MultiColumnFlowThread();

  // A column out of range doesn't have a flow thread portion, so we need to
  // clamp to make sure that we stay within the actual columns. This means that
  // content in the overflow area will be mapped to the last actual column,
  // instead of being mapped to an imaginary column further ahead.
  unsigned column_index =
      offset_in_flow_thread >= LogicalBottomInFlowThread()
          ? ActualColumnCount() - 1
          : ColumnIndexAtOffset(offset_in_flow_thread, rule);

  LayoutRect portion_rect(FlowThreadPortionRectAt(column_index));
  flow_thread->DeprecatedFlipForWritingMode(portion_rect);
  portion_rect.MoveBy(flow_thread->PhysicalLocation().ToLayoutPoint());

  LayoutRect column_rect(ColumnRectAt(column_index));
  column_rect.Move(OffsetFromColumnSet());
  column_set_.DeprecatedFlipForWritingMode(column_rect);
  column_rect.MoveBy(column_set_.PhysicalLocation().ToLayoutPoint());

  LayoutSize translation_relative_to_flow_thread =
      column_rect.Location() - portion_rect.Location();
  if (mode == CoordinateSpaceConversion::kContaining)
    return translation_relative_to_flow_thread;

  LayoutSize enclosing_translation;
  if (LayoutMultiColumnFlowThread* enclosing_flow_thread =
          flow_thread->EnclosingFlowThread()) {
    const MultiColumnFragmentainerGroup& first_row =
        flow_thread->FirstMultiColumnSet()->FirstFragmentainerGroup();
    // Translation that would map points in the coordinate space of the
    // outermost flow thread to visual points in the first column in the first
    // fragmentainer group (row) in our multicol container.
    LayoutSize enclosing_translation_origin =
        enclosing_flow_thread->FlowThreadTranslationAtOffset(
            first_row.BlockOffsetInEnclosingFragmentationContext(),
            LayoutBox::kAssociateWithLatterPage, mode);

    // Translation that would map points in the coordinate space of the
    // outermost flow thread to visual points in the first column in this
    // fragmentainer group.
    enclosing_translation =
        enclosing_flow_thread->FlowThreadTranslationAtOffset(
            BlockOffsetInEnclosingFragmentationContext(),
            LayoutBox::kAssociateWithLatterPage, mode);

    // What we ultimately return from this method is a translation that maps
    // points in the coordinate space of our flow thread to a visual point in a
    // certain column in this fragmentainer group. We had to go all the way up
    // to the outermost flow thread, since this fragmentainer group may be in a
    // different outer column than the first outer column that this multicol
    // container lives in. It's the visual distance between the first
    // fragmentainer group and this fragmentainer group that we need to add to
    // the translation.
    enclosing_translation -= enclosing_translation_origin;
  }

  return enclosing_translation + translation_relative_to_flow_thread;
}

LayoutUnit MultiColumnFragmentainerGroup::ColumnLogicalTopForOffset(
    LayoutUnit offset_in_flow_thread) const {
  unsigned column_index = ColumnIndexAtOffset(
      offset_in_flow_thread, LayoutBox::kAssociateWithLatterPage);
  return LogicalTopInFlowThreadAt(column_index);
}

LayoutPoint MultiColumnFragmentainerGroup::VisualPointToFlowThreadPoint(
    const LayoutPoint& visual_point,
    SnapToColumnPolicy snap) const {
  unsigned column_index = ColumnIndexAtVisualPoint(visual_point);
  LayoutRect column_rect = ColumnRectAt(column_index);
  LayoutPoint local_point(visual_point);
  local_point.MoveBy(-column_rect.Location());
  if (!column_set_.IsHorizontalWritingMode()) {
    if (snap == kSnapToColumn) {
      LayoutUnit column_start = column_set_.StyleRef().IsLeftToRightDirection()
                                    ? LayoutUnit()
                                    : column_rect.Height();
      if (local_point.X() < 0)
        local_point = LayoutPoint(LayoutUnit(), column_start);
      else if (local_point.X() > ColumnLogicalHeight())
        local_point = LayoutPoint(ColumnLogicalHeight(), column_start);
    }
    return LayoutPoint(local_point.X() + LogicalTopInFlowThreadAt(column_index),
                       local_point.Y());
  }
  if (snap == kSnapToColumn) {
    LayoutUnit column_start = column_set_.StyleRef().IsLeftToRightDirection()
                                  ? LayoutUnit()
                                  : column_rect.Width();
    if (local_point.Y() < 0)
      local_point = LayoutPoint(column_start, LayoutUnit());
    else if (local_point.Y() > ColumnLogicalHeight())
      local_point = LayoutPoint(column_start, ColumnLogicalHeight());
  }
  return LayoutPoint(local_point.X(),
                     local_point.Y() + LogicalTopInFlowThreadAt(column_index));
}

LayoutRect MultiColumnFragmentainerGroup::FragmentsBoundingBox(
    const LayoutRect& bounding_box_in_flow_thread) const {
  // Find the start and end column intersected by the bounding box.
  LayoutRect flipped_bounding_box_in_flow_thread(bounding_box_in_flow_thread);
  LayoutFlowThread* flow_thread = column_set_.FlowThread();
  flow_thread->DeprecatedFlipForWritingMode(
      flipped_bounding_box_in_flow_thread);
  bool is_horizontal_writing_mode = column_set_.IsHorizontalWritingMode();
  LayoutUnit bounding_box_logical_top =
      is_horizontal_writing_mode ? flipped_bounding_box_in_flow_thread.Y()
                                 : flipped_bounding_box_in_flow_thread.X();
  LayoutUnit bounding_box_logical_bottom =
      is_horizontal_writing_mode ? flipped_bounding_box_in_flow_thread.MaxY()
                                 : flipped_bounding_box_in_flow_thread.MaxX();
  if (bounding_box_logical_bottom <= LogicalTopInFlowThread() ||
      bounding_box_logical_top >= LogicalBottomInFlowThread()) {
    // The bounding box doesn't intersect this fragmentainer group.
    return LayoutRect();
  }
  unsigned start_column;
  unsigned end_column;
  ColumnIntervalForBlockRangeInFlowThread(bounding_box_logical_top,
                                          bounding_box_logical_bottom,
                                          start_column, end_column);

  LayoutRect start_column_flow_thread_overflow_portion =
      FlowThreadPortionOverflowRectAt(start_column);
  flow_thread->DeprecatedFlipForWritingMode(
      start_column_flow_thread_overflow_portion);
  LayoutRect start_column_rect(bounding_box_in_flow_thread);
  start_column_rect.Intersect(start_column_flow_thread_overflow_portion);
  start_column_rect.Move(
      FlowThreadTranslationAtOffset(LogicalTopInFlowThreadAt(start_column),
                                    LayoutBox::kAssociateWithLatterPage,
                                    CoordinateSpaceConversion::kContaining));
  if (start_column == end_column)
    return start_column_rect;  // It all takes place in one column. We're done.

  LayoutRect end_column_flow_thread_overflow_portion =
      FlowThreadPortionOverflowRectAt(end_column);
  flow_thread->DeprecatedFlipForWritingMode(
      end_column_flow_thread_overflow_portion);
  LayoutRect end_column_rect(bounding_box_in_flow_thread);
  end_column_rect.Intersect(end_column_flow_thread_overflow_portion);
  end_column_rect.Move(FlowThreadTranslationAtOffset(
      LogicalTopInFlowThreadAt(end_column), LayoutBox::kAssociateWithLatterPage,
      CoordinateSpaceConversion::kContaining));
  return UnionRect(start_column_rect, end_column_rect);
}

LayoutRect MultiColumnFragmentainerGroup::CalculateOverflow() const {
  // Note that we just return the bounding rectangle of the column boxes here.
  // We currently don't examine overflow caused by the actual content that ends
  // up in each column.
  LayoutRect overflow_rect;
  if (unsigned column_count = ActualColumnCount()) {
    overflow_rect = ColumnRectAt(0);
    if (column_count > 1)
      overflow_rect.UniteEvenIfEmpty(ColumnRectAt(column_count - 1));
  }
  return overflow_rect;
}

unsigned MultiColumnFragmentainerGroup::ActualColumnCount() const {
  unsigned count = UnclampedActualColumnCount();
  count = std::min(count, kColumnCountClampMax);
  DCHECK_GE(count, 1u);
  return count;
}

void MultiColumnFragmentainerGroup::UpdateFromNG(LayoutUnit logical_height) {
  logical_height_ = logical_height;
  is_logical_height_known_ = true;
}

LayoutUnit MultiColumnFragmentainerGroup::HeightAdjustedForRowOffset(
    LayoutUnit height) const {
  LayoutUnit adjusted_height =
      height - LogicalTop() - column_set_.LogicalTopFromMulticolContentEdge();
  return adjusted_height.ClampNegativeToZero();
}

LayoutUnit MultiColumnFragmentainerGroup::CalculateMaxColumnHeight() const {
  LayoutMultiColumnFlowThread* flow_thread =
      column_set_.MultiColumnFlowThread();
  LayoutUnit max_column_height = flow_thread->MaxColumnLogicalHeight();
  LayoutUnit max_height = HeightAdjustedForRowOffset(max_column_height);
  if (FragmentationContext* enclosing_fragmentation_context =
          flow_thread->EnclosingFragmentationContext()) {
    if (enclosing_fragmentation_context->IsFragmentainerLogicalHeightKnown()) {
      // We're nested inside another fragmentation context whose fragmentainer
      // heights are known. This constrains the max height.
      LayoutUnit remaining_outer_logical_height =
          enclosing_fragmentation_context->RemainingLogicalHeightAt(
              BlockOffsetInEnclosingFragmentationContext());
      if (max_height > remaining_outer_logical_height)
        max_height = remaining_outer_logical_height;
    }
  }
  return max_height;
}

void MultiColumnFragmentainerGroup::SetAndConstrainColumnHeight(
    LayoutUnit new_height) {
  logical_height_ = new_height;
  if (logical_height_ > max_logical_height_)
    logical_height_ = max_logical_height_;
}

LayoutUnit MultiColumnFragmentainerGroup::RebalanceColumnHeightIfNeeded()
    const {
  if (ActualColumnCount() <= column_set_.UsedColumnCount()) {
    // With the current column height, the content fits without creating
    // overflowing columns. We're done.
    return logical_height_;
  }

  if (logical_height_ >= max_logical_height_) {
    // We cannot stretch any further. We'll just have to live with the
    // overflowing columns. This typically happens if the max column height is
    // less than the height of the tallest piece of unbreakable content (e.g.
    // lines).
    return logical_height_;
  }

  MinimumSpaceShortageFinder shortage_finder(
      ColumnSet(), LogicalTopInFlowThread(), LogicalBottomInFlowThread());

  if (shortage_finder.ForcedBreaksCount() + 1 >=
      column_set_.UsedColumnCount()) {
    // Too many forced breaks to allow any implicit breaks. Initial balancing
    // should already have set a good height. There's nothing more we should do.
    return logical_height_;
  }

  // If the initial guessed column height wasn't enough, stretch it now. Stretch
  // by the lowest amount of space.
  LayoutUnit min_space_shortage = shortage_finder.MinimumSpaceShortage();

  DCHECK_GT(min_space_shortage, 0);  // We should never _shrink_ the height!

  if (min_space_shortage == LayoutUnit::Max()) {
    // We failed to find an amount to stretch the columns by. This is a bug; see
    // e.g. crbug.com/510340 . If this happens, though, we need bail out rather
    // than looping infinitely.
    return logical_height_;
  }

  return logical_height_ + min_space_shortage;
}

LayoutRect MultiColumnFragmentainerGroup::ColumnRectAt(
    unsigned column_index) const {
  LayoutUnit column_logical_width = column_set_.PageLogicalWidth();
  LayoutUnit column_logical_height = LogicalHeightInFlowThreadAt(column_index);
  LayoutUnit column_logical_top;
  LayoutUnit column_logical_left;
  LayoutUnit column_gap = column_set_.ColumnGap();

  if (column_set_.StyleRef().IsLeftToRightDirection()) {
    column_logical_left += column_index * (column_logical_width + column_gap);
  } else {
    column_logical_left += column_set_.ContentLogicalWidth() -
                           column_logical_width -
                           column_index * (column_logical_width + column_gap);
  }

  LayoutRect column_rect(column_logical_left, column_logical_top,
                         column_logical_width, column_logical_height);
  if (!column_set_.IsHorizontalWritingMode())
    return column_rect.TransposedRect();
  return column_rect;
}

LayoutRect MultiColumnFragmentainerGroup::FlowThreadPortionRectAt(
    unsigned column_index) const {
  LayoutUnit logical_top = LogicalTopInFlowThreadAt(column_index);
  LayoutUnit portion_logical_height = LogicalHeightInFlowThreadAt(column_index);
  if (column_set_.IsHorizontalWritingMode())
    return LayoutRect(LayoutUnit(), logical_top, column_set_.PageLogicalWidth(),
                      portion_logical_height);
  return LayoutRect(logical_top, LayoutUnit(), portion_logical_height,
                    column_set_.PageLogicalWidth());
}

LayoutRect MultiColumnFragmentainerGroup::FlowThreadPortionOverflowRectAt(
    unsigned column_index) const {
  // This function determines the portion of the flow thread that paints for the
  // column.
  //
  // In the block direction, we will not clip overflow out of the top of the
  // first column, or out of the bottom of the last column. This applies only to
  // the true first column and last column across all column sets.
  //
  // FIXME: Eventually we will know overflow on a per-column basis, but we can't
  // do this until we have a painting mode that understands not to paint
  // contents from a previous column in the overflow area of a following column.
  bool is_first_column_in_row = !column_index;
  bool is_last_column_in_row = column_index == ActualColumnCount() - 1;

  LayoutRect portion_rect = FlowThreadPortionRectAt(column_index);
  bool is_first_column_in_multicol_container =
      is_first_column_in_row &&
      this == &column_set_.FirstFragmentainerGroup() &&
      !column_set_.PreviousSiblingMultiColumnSet();
  bool is_last_column_in_multicol_container =
      is_last_column_in_row && this == &column_set_.LastFragmentainerGroup() &&
      !column_set_.NextSiblingMultiColumnSet();
  // Calculate the overflow rectangle. It will be clipped at the logical top
  // and bottom of the column box, unless it's the first or last column in the
  // multicol container, in which case it should allow overflow. It will also
  // be clipped in the middle of adjacent column gaps. Care is taken here to
  // avoid rounding errors.
  LayoutRect overflow_rect(
      IntRect(-kMulticolMaxClipPixels, -kMulticolMaxClipPixels,
              2 * kMulticolMaxClipPixels, 2 * kMulticolMaxClipPixels));
  if (column_set_.IsHorizontalWritingMode()) {
    if (!is_first_column_in_multicol_container)
      overflow_rect.ShiftYEdgeTo(portion_rect.Y());
    if (!is_last_column_in_multicol_container)
      overflow_rect.ShiftMaxYEdgeTo(portion_rect.MaxY());
  } else {
    if (!is_first_column_in_multicol_container)
      overflow_rect.ShiftXEdgeTo(portion_rect.X());
    if (!is_last_column_in_multicol_container)
      overflow_rect.ShiftMaxXEdgeTo(portion_rect.MaxX());
  }
  return overflow_rect;
}

unsigned MultiColumnFragmentainerGroup::ColumnIndexAtOffset(
    LayoutUnit offset_in_flow_thread,
    LayoutBox::PageBoundaryRule page_boundary_rule) const {
  // Handle the offset being out of range.
  if (offset_in_flow_thread < logical_top_in_flow_thread_)
    return 0;

  if (!IsLogicalHeightKnown())
    return 0;
  LayoutUnit column_height = ColumnLogicalHeight();
  unsigned column_index =
      ((offset_in_flow_thread - logical_top_in_flow_thread_) / column_height)
          .Floor();
  if (page_boundary_rule == LayoutBox::kAssociateWithFormerPage &&
      column_index > 0 &&
      LogicalTopInFlowThreadAt(column_index) == offset_in_flow_thread) {
    // We are exactly at a column boundary, and we've been told to associate
    // offsets at column boundaries with the former column, not the latter.
    column_index--;
  }
  return column_index;
}

unsigned MultiColumnFragmentainerGroup::ConstrainedColumnIndexAtOffset(
    LayoutUnit offset_in_flow_thread,
    LayoutBox::PageBoundaryRule page_boundary_rule) const {
  unsigned index =
      ColumnIndexAtOffset(offset_in_flow_thread, page_boundary_rule);
  return std::min(index, ActualColumnCount() - 1);
}

unsigned MultiColumnFragmentainerGroup::ColumnIndexAtVisualPoint(
    const LayoutPoint& visual_point) const {
  LayoutUnit column_length = column_set_.PageLogicalWidth();
  LayoutUnit offset_in_column_progression_direction =
      column_set_.IsHorizontalWritingMode() ? visual_point.X()
                                            : visual_point.Y();
  if (!column_set_.StyleRef().IsLeftToRightDirection()) {
    offset_in_column_progression_direction =
        column_set_.LogicalWidth() - offset_in_column_progression_direction;
  }
  LayoutUnit column_gap = column_set_.ColumnGap();
  if (column_length + column_gap <= 0)
    return 0;
  // Column boundaries are in the middle of the column gap.
  int index = ((offset_in_column_progression_direction + column_gap / 2) /
               (column_length + column_gap))
                  .ToInt();
  if (index < 0)
    return 0;
  return std::min(unsigned(index), ActualColumnCount() - 1);
}

void MultiColumnFragmentainerGroup::ColumnIntervalForBlockRangeInFlowThread(
    LayoutUnit logical_top_in_flow_thread,
    LayoutUnit logical_bottom_in_flow_thread,
    unsigned& first_column,
    unsigned& last_column) const {
  logical_top_in_flow_thread =
      std::max(logical_top_in_flow_thread, LogicalTopInFlowThread());
  logical_bottom_in_flow_thread =
      std::min(logical_bottom_in_flow_thread, LogicalBottomInFlowThread());
  first_column = ConstrainedColumnIndexAtOffset(
      logical_top_in_flow_thread, LayoutBox::kAssociateWithLatterPage);
  if (logical_bottom_in_flow_thread <= logical_top_in_flow_thread) {
    // Zero-height block range. There'll be one column in the interval. Set it
    // right away. This is important if we're at a column boundary, since
    // calling ConstrainedColumnIndexAtOffset() with the end-exclusive bottom
    // offset would actually give us the *previous* column.
    last_column = first_column;
  } else {
    last_column = ConstrainedColumnIndexAtOffset(
        logical_bottom_in_flow_thread, LayoutBox::kAssociateWithFormerPage);
  }
}

void MultiColumnFragmentainerGroup::ColumnIntervalForVisualRect(
    const LayoutRect& rect,
    unsigned& first_column,
    unsigned& last_column) const {
  bool is_column_ltr = column_set_.StyleRef().IsLeftToRightDirection();
  if (column_set_.IsHorizontalWritingMode()) {
    if (is_column_ltr) {
      first_column = ColumnIndexAtVisualPoint(rect.MinXMinYCorner());
      last_column = ColumnIndexAtVisualPoint(rect.MaxXMinYCorner());
    } else {
      first_column = ColumnIndexAtVisualPoint(rect.MaxXMinYCorner());
      last_column = ColumnIndexAtVisualPoint(rect.MinXMinYCorner());
    }
  } else {
    if (is_column_ltr) {
      first_column = ColumnIndexAtVisualPoint(rect.MinXMinYCorner());
      last_column = ColumnIndexAtVisualPoint(rect.MinXMaxYCorner());
    } else {
      first_column = ColumnIndexAtVisualPoint(rect.MinXMaxYCorner());
      last_column = ColumnIndexAtVisualPoint(rect.MinXMinYCorner());
    }
  }
  DCHECK_LE(first_column, last_column);
}

unsigned MultiColumnFragmentainerGroup::UnclampedActualColumnCount() const {
  // We must always return a value of 1 or greater. Column count = 0 is a
  // meaningless situation, and will confuse and cause problems in other parts
  // of the code.
  if (!IsLogicalHeightKnown())
    return 1;
  // Our flow thread portion determines our column count. We have as many
  // columns as needed to fit all the content.
  LayoutUnit flow_thread_portion_height = LogicalHeightInFlowThread();
  if (!flow_thread_portion_height)
    return 1;

  LayoutUnit column_height = ColumnLogicalHeight();
  unsigned count = (flow_thread_portion_height / column_height).Floor();
  // flowThreadPortionHeight may be saturated, so detect the remainder manually.
  if (count * column_height < flow_thread_portion_height)
    count++;

  DCHECK_GE(count, 1u);
  return count;
}

MultiColumnFragmentainerGroupList::MultiColumnFragmentainerGroupList(
    LayoutMultiColumnSet& column_set)
    : column_set_(column_set) {
  Append(MultiColumnFragmentainerGroup(column_set_));
}

// An explicit empty destructor of MultiColumnFragmentainerGroupList should be
// in MultiColumnFragmentainerGroup.cpp, because if an implicit destructor is
// used, msvc 2015 tries to generate its destructor (because the class is
// dll-exported class) and causes a compile error because of lack of
// MultiColumnFragmentainerGroup::operator=.  Since
// MultiColumnFragmentainerGroup is non-copyable, we cannot define the
// operator=.
MultiColumnFragmentainerGroupList::~MultiColumnFragmentainerGroupList() =
    default;

MultiColumnFragmentainerGroup&
MultiColumnFragmentainerGroupList::AddExtraGroup() {
  Append(MultiColumnFragmentainerGroup(column_set_));
  return Last();
}

void MultiColumnFragmentainerGroupList::DeleteExtraGroups() {
  Shrink(1);
}

}  // namespace blink
