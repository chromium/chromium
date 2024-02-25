// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/fragmentainer_iterator.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"

namespace blink {

FragmentainerIterator::FragmentainerIterator(
    const LayoutFlowThread& flow_thread,
    const PhysicalRect& physical_bounding_box_in_flow_thread)
    : current_fragmentainer_group_index_(0) {
  LogicalRect bounds_in_flow_thread =
      flow_thread.CreateWritingModeConverter().ToLogical(
          physical_bounding_box_in_flow_thread);

  logical_top_in_flow_thread_ = bounds_in_flow_thread.offset.block_offset;
  logical_bottom_in_flow_thread_ = bounds_in_flow_thread.BlockEndOffset();
  bounding_box_is_empty_ = bounds_in_flow_thread.IsEmpty();

  // Jump to the first interesting column set.
  current_column_set_ = flow_thread.ColumnSetAtBlockOffset(
      logical_top_in_flow_thread_, LayoutBox::kAssociateWithLatterPage);
  if (!current_column_set_) {
    SetAtEnd();
    return;
  }
  // Then find the first interesting fragmentainer group.
  current_fragmentainer_group_index_ =
      current_column_set_->FragmentainerGroupIndexAtFlowThreadOffset(
          logical_top_in_flow_thread_, LayoutBox::kAssociateWithLatterPage);

  // Now find the first and last fragmentainer we're interested in.
  SetFragmentainersOfInterest();
}

void FragmentainerIterator::Advance() {
  DCHECK(!AtEnd());

  if (current_fragmentainer_index_ < end_fragmentainer_index_) {
    current_fragmentainer_index_++;
  } else {
    // That was the last fragmentainer to visit in this fragmentainer group.
    // Advance to the next group.
    MoveToNextFragmentainerGroup();
    if (AtEnd())
      return;
  }
}

PhysicalRect FragmentainerIterator::ClipRectInFlowThread() const {
  DCHECK(!AtEnd());
  PhysicalRect clip_rect;
  // An empty bounding box rect would typically be 0,0 0x0, so it would be
  // placed in the first column always. However, the first column might not have
  // a top edge clip (see FlowThreadPortionOverflowRectAt()). This might cause
  // artifacts to paint outside of the column container. To avoid this
  // situation, and since the logical bounding box is empty anyway, use the
  // portion rect instead which is bounded on all sides. Note that we don't
  // return an empty clip here, because an empty clip indicates that we have an
  // empty column which may be treated differently by the calling code.
  if (bounding_box_is_empty_) {
    clip_rect =
        CurrentGroup().FlowThreadPortionRectAt(current_fragmentainer_index_);
  } else {
    clip_rect = CurrentGroup().FlowThreadPortionOverflowRectAt(
        current_fragmentainer_index_);
  }
  return clip_rect;
}

const MultiColumnFragmentainerGroup& FragmentainerIterator::CurrentGroup()
    const {
  DCHECK(!AtEnd());
  return current_column_set_
      ->FragmentainerGroups()[current_fragmentainer_group_index_];
}

void FragmentainerIterator::MoveToNextFragmentainerGroup() {
  current_fragmentainer_group_index_++;
  if (current_fragmentainer_group_index_ >=
      current_column_set_->FragmentainerGroups().size()) {
    // That was the last fragmentainer group in this set. Advance to the next.
    current_column_set_ = current_column_set_->NextSiblingMultiColumnSet();
    current_fragmentainer_group_index_ = 0;
    if (!current_column_set_ || current_column_set_->LogicalTopInFlowThread() >=
                                    logical_bottom_in_flow_thread_) {
      SetAtEnd();
      return;  // No more sets or next set out of range. We're done.
    }
  }
  if (CurrentGroup().LogicalTopInFlowThread() >=
      logical_bottom_in_flow_thread_) {
    // This fragmentainer group doesn't intersect with the range we're
    // interested in. We're done.
    SetAtEnd();
    return;
  }
  SetFragmentainersOfInterest();
}

void FragmentainerIterator::SetFragmentainersOfInterest() {
  const MultiColumnFragmentainerGroup& group = CurrentGroup();

  // Figure out the start and end fragmentainers for the block range we're
  // interested in. We might not have to walk the entire fragmentainer group.
  group.ColumnIntervalForBlockRangeInFlowThread(
      logical_top_in_flow_thread_, logical_bottom_in_flow_thread_,
      current_fragmentainer_index_, end_fragmentainer_index_);
  DCHECK_GE(end_fragmentainer_index_, current_fragmentainer_index_);
}

}  // namespace blink
