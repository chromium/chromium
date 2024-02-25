// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"

namespace blink {

// Limit the maximum column count, to prevent potential performance problems.
static const unsigned kColumnCountClampMax = 10000;

// Clamp "infinite" clips to a number of pixels that can be losslessly
// converted to and from floating point, to avoid loss of precision.
// Note that tables have something similar, see
// TableLayoutAlgorithm::kTableMaxWidth.
static constexpr LayoutUnit kMulticolMaxClipPixels(1000000);

MultiColumnFragmentainerGroup::MultiColumnFragmentainerGroup(
    const LayoutMultiColumnSet& column_set)
    : column_set_(&column_set) {}

LogicalOffset MultiColumnFragmentainerGroup::OffsetFromColumnSet() const {
  return LogicalOffset(LayoutUnit(), LogicalTop());
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
  is_logical_height_known_ = false;
  logical_height_ = LayoutUnit();
}

PhysicalOffset MultiColumnFragmentainerGroup::FlowThreadTranslationAtOffset(
    LayoutUnit offset_in_flow_thread,
    LayoutBox::PageBoundaryRule rule) const {
  LayoutMultiColumnFlowThread* flow_thread =
      column_set_->MultiColumnFlowThread();

  // A column out of range doesn't have a flow thread portion, so we need to
  // clamp to make sure that we stay within the actual columns. This means that
  // content in the overflow area will be mapped to the last actual column,
  // instead of being mapped to an imaginary column further ahead.
  unsigned column_index =
      offset_in_flow_thread >= LogicalBottomInFlowThread()
          ? ActualColumnCount() - 1
          : ColumnIndexAtOffset(offset_in_flow_thread, rule);

  PhysicalRect portion_rect(FlowThreadPortionRectAt(column_index));
  portion_rect.offset += flow_thread->PhysicalLocation();

  LogicalRect column_rect(ColumnRectAt(column_index));
  column_rect.offset += OffsetFromColumnSet();
  PhysicalRect physical_column_rect =
      column_set_->CreateWritingModeConverter().ToPhysical(column_rect);
  physical_column_rect.offset += column_set_->PhysicalLocation();

  return physical_column_rect.offset - portion_rect.offset;
}

LogicalOffset MultiColumnFragmentainerGroup::VisualPointToFlowThreadPoint(
    const LogicalOffset& visual_point) const {
  unsigned column_index = ColumnIndexAtVisualPoint(visual_point);
  LogicalRect column_rect = ColumnRectAt(column_index);
  LogicalOffset local_point(visual_point);
  local_point -= column_rect.offset;
  return LogicalOffset(
      local_point.inline_offset,
      local_point.block_offset + LogicalTopInFlowThreadAt(column_index));
}

PhysicalRect MultiColumnFragmentainerGroup::FragmentsBoundingBox(
    const PhysicalRect& bounding_box_in_flow_thread) const {
  // Find the start and end column intersected by the bounding box.
  const LogicalRect logical_bounding_box =
      column_set_->FlowThread()->CreateWritingModeConverter().ToLogical(
          bounding_box_in_flow_thread);
  LayoutUnit bounding_box_logical_top =
      logical_bounding_box.offset.block_offset;
  LayoutUnit bounding_box_logical_bottom =
      logical_bounding_box.BlockEndOffset();
  if (bounding_box_logical_bottom <= LogicalTopInFlowThread() ||
      bounding_box_logical_top >= LogicalBottomInFlowThread()) {
    // The bounding box doesn't intersect this fragmentainer group.
    return PhysicalRect();
  }
  unsigned start_column;
  unsigned end_column;
  ColumnIntervalForBlockRangeInFlowThread(bounding_box_logical_top,
                                          bounding_box_logical_bottom,
                                          start_column, end_column);

  PhysicalRect start_column_rect(bounding_box_in_flow_thread);
  start_column_rect.Intersect(FlowThreadPortionOverflowRectAt(start_column));
  start_column_rect.offset += PhysicalOffset(
      FlowThreadTranslationAtOffset(LogicalTopInFlowThreadAt(start_column),
                                    LayoutBox::kAssociateWithLatterPage));
  if (start_column == end_column)
    return start_column_rect;  // It all takes place in one column. We're done.

  PhysicalRect end_column_rect(bounding_box_in_flow_thread);
  end_column_rect.Intersect(FlowThreadPortionOverflowRectAt(end_column));
  end_column_rect.offset += PhysicalOffset(
      FlowThreadTranslationAtOffset(LogicalTopInFlowThreadAt(end_column),
                                    LayoutBox::kAssociateWithLatterPage));
  return UnionRect(start_column_rect, end_column_rect);
}

unsigned MultiColumnFragmentainerGroup::ActualColumnCount() const {
  unsigned count = UnclampedActualColumnCount();
  count = std::min(count, kColumnCountClampMax);
  DCHECK_GE(count, 1u);
  return count;
}

void MultiColumnFragmentainerGroup::SetColumnBlockSizeFromNG(
    LayoutUnit block_size) {
  // We clamp the fragmentainer block size up to 1 for legacy write-back if
  // there is content that overflows the less-than-1px-height (or even
  // zero-height) fragmentainer. However, if one fragmentainer contains no
  // overflow, while others fragmentainers do, the known height may be different
  // than the |block_size| passed in. Don't override the stored height if this
  // is the case.
  DCHECK(!is_logical_height_known_ || logical_height_ == block_size ||
         block_size <= LayoutUnit(1));
  if (is_logical_height_known_)
    return;
  logical_height_ = block_size;
  is_logical_height_known_ = true;
}

void MultiColumnFragmentainerGroup::ExtendColumnBlockSizeFromNG(
    LayoutUnit block_size) {
  DCHECK(is_logical_height_known_);
  logical_height_ += block_size;
}

LogicalRect MultiColumnFragmentainerGroup::ColumnRectAt(
    unsigned column_index) const {
  LayoutUnit column_logical_width = column_set_->PageLogicalWidth();
  LayoutUnit column_logical_height = LogicalHeightInFlowThreadAt(column_index);
  LayoutUnit column_logical_top;
  LayoutUnit column_logical_left;
  LayoutUnit column_gap = column_set_->ColumnGap();

  if (column_set_->StyleRef().IsLeftToRightDirection()) {
    column_logical_left += column_index * (column_logical_width + column_gap);
  } else {
    column_logical_left += column_set_->ContentLogicalWidth() -
                           column_logical_width -
                           column_index * (column_logical_width + column_gap);
  }

  return LogicalRect(column_logical_left, column_logical_top,
                     column_logical_width, column_logical_height);
}

LogicalRect MultiColumnFragmentainerGroup::LogicalFlowThreadPortionRectAt(
    unsigned column_index) const {
  LayoutUnit logical_top = LogicalTopInFlowThreadAt(column_index);
  LayoutUnit portion_logical_height = LogicalHeightInFlowThreadAt(column_index);
  return LogicalRect(LayoutUnit(), logical_top, column_set_->PageLogicalWidth(),
                     portion_logical_height);
}

PhysicalRect MultiColumnFragmentainerGroup::FlowThreadPortionRectAt(
    unsigned column_index) const {
  return column_set_->FlowThread()->CreateWritingModeConverter().ToPhysical(
      LogicalFlowThreadPortionRectAt(column_index));
}

PhysicalRect MultiColumnFragmentainerGroup::FlowThreadPortionOverflowRectAt(
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

  LogicalRect portion_rect = LogicalFlowThreadPortionRectAt(column_index);
  bool is_first_column_in_multicol_container =
      is_first_column_in_row &&
      this == &column_set_->FirstFragmentainerGroup() &&
      !column_set_->PreviousSiblingMultiColumnSet();
  bool is_last_column_in_multicol_container =
      is_last_column_in_row && this == &column_set_->LastFragmentainerGroup() &&
      !column_set_->NextSiblingMultiColumnSet();
  // Calculate the overflow rectangle. It will be clipped at the logical top
  // and bottom of the column box, unless it's the first or last column in the
  // multicol container, in which case it should allow overflow. It will also
  // be clipped in the middle of adjacent column gaps. Care is taken here to
  // avoid rounding errors.
  LogicalRect overflow_rect(-kMulticolMaxClipPixels, -kMulticolMaxClipPixels,
                            2 * kMulticolMaxClipPixels,
                            2 * kMulticolMaxClipPixels);
  if (!is_first_column_in_multicol_container) {
    overflow_rect.ShiftBlockStartEdgeTo(portion_rect.offset.block_offset);
  }
  if (!is_last_column_in_multicol_container) {
    overflow_rect.ShiftBlockEndEdgeTo(portion_rect.BlockEndOffset());
  }
  return column_set_->FlowThread()->CreateWritingModeConverter().ToPhysical(
      overflow_rect);
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
    const LogicalOffset& visual_point) const {
  LayoutUnit column_length = column_set_->PageLogicalWidth();
  LayoutUnit offset_in_column_progression_direction =
      visual_point.inline_offset;
  if (!column_set_->StyleRef().IsLeftToRightDirection()) {
    offset_in_column_progression_direction =
        column_set_->LogicalWidth() - offset_in_column_progression_direction;
  }
  LayoutUnit column_gap = column_set_->ColumnGap();
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

void MultiColumnFragmentainerGroup::Trace(Visitor* visitor) const {
  visitor->Trace(column_set_);
}

MultiColumnFragmentainerGroupList::MultiColumnFragmentainerGroupList(
    LayoutMultiColumnSet& column_set)
    : column_set_(&column_set) {
  Append(MultiColumnFragmentainerGroup(*column_set_));
}

// An explicit empty destructor of MultiColumnFragmentainerGroupList should be
// in multi_column_fragmentainer_group.cc, because if an implicit destructor is
// used, msvc 2015 tries to generate its destructor (because the class is
// dll-exported class) and causes a compile error because of lack of
// MultiColumnFragmentainerGroup::operator=.  Since
// MultiColumnFragmentainerGroup is non-copyable, we cannot define the
// operator=.
MultiColumnFragmentainerGroupList::~MultiColumnFragmentainerGroupList() =
    default;

MultiColumnFragmentainerGroup&
MultiColumnFragmentainerGroupList::AddExtraGroup() {
  Append(MultiColumnFragmentainerGroup(*column_set_));
  return Last();
}

void MultiColumnFragmentainerGroupList::DeleteExtraGroups() {
  Shrink(1);
}

void MultiColumnFragmentainerGroupList::Trace(Visitor* visitor) const {
  visitor->Trace(column_set_);
  visitor->Trace(groups_);
}

}  // namespace blink
