// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_baseline_alignment.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

// This function gives the margin 'over' based on the baseline-axis,
// since in grid we can can 2-dimensional alignment by baseline. In
// horizontal writing-mode, the row-axis is the horizontal axis. When
// we use this axis to move the grid items so that they are
// baseline-aligned, we want their "horizontal" margin (right); the
// same will happen when using the column-axis under vertical writing
// mode, we also want in this case the 'right' margin.
LayoutUnit GridBaselineAlignment::MarginOverForChild(const LayoutBox& child,
                                                     GridAxis axis) const {
  return IsHorizontalBaselineAxis(axis) ? child.MarginRight()
                                        : child.MarginTop();
}

// This function gives the margin 'under' based on the baseline-axis,
// since in grid we can can 2-dimensional alignment by baseline. In
// horizontal writing-mode, the row-axis is the horizontal axis. When
// we use this axis to move the grid items so that they are
// baseline-aligned, we want their "horizontal" margin (left); the
// same will happen when using the column-axis under vertical writing
// mode, we also want in this case the 'left' margin.
LayoutUnit GridBaselineAlignment::MarginUnderForChild(const LayoutBox& child,
                                                      GridAxis axis) const {
  return IsHorizontalBaselineAxis(axis) ? child.MarginLeft()
                                        : child.MarginBottom();
}

LayoutUnit GridBaselineAlignment::LogicalAscentForChild(
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  LayoutUnit ascent = AscentForChild(child, baseline_axis);
  return IsDescentBaselineForChild(child, baseline_axis)
             ? DescentForChild(child, ascent, baseline_axis)
             : ascent;
}

LayoutUnit GridBaselineAlignment::AscentForChild(const LayoutBox& child,
                                                 GridAxis baseline_axis) const {
  LayoutUnit margin = IsDescentBaselineForChild(child, baseline_axis)
                          ? MarginUnderForChild(child, baseline_axis)
                          : MarginOverForChild(child, baseline_axis);
  LayoutUnit baseline = IsParallelToBaselineAxisForChild(child, baseline_axis)
                            ? child.FirstLineBoxBaseline()
                            : LayoutUnit(-1);
  // We take border-box's under edge if no valid baseline.
  if (baseline == -1) {
    if (IsHorizontalBaselineAxis(baseline_axis)) {
      return IsFlippedBlocksWritingMode(block_flow_)
                 ? child.Size().Width().ToInt() + margin
                 : margin;
    }
    return child.Size().Height() + margin;
  }
  return baseline + margin;
}

LayoutUnit GridBaselineAlignment::DescentForChild(
    const LayoutBox& child,
    LayoutUnit ascent,
    GridAxis baseline_axis) const {
  if (IsParallelToBaselineAxisForChild(child, baseline_axis))
    return child.MarginLogicalHeight() + child.LogicalHeight() - ascent;
  return child.MarginLogicalWidth() + child.LogicalWidth() - ascent;
}

bool GridBaselineAlignment::IsDescentBaselineForChild(
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  return IsHorizontalBaselineAxis(baseline_axis) &&
         ((child.StyleRef().IsFlippedBlocksWritingMode() &&
           !IsFlippedBlocksWritingMode(block_flow_)) ||
          (child.StyleRef().IsFlippedLinesWritingMode() &&
           IsFlippedBlocksWritingMode(block_flow_)));
}

bool GridBaselineAlignment::IsHorizontalBaselineAxis(GridAxis axis) const {
  return axis == kGridRowAxis ? IsHorizontalWritingMode(block_flow_)
                              : !IsHorizontalWritingMode(block_flow_);
}

bool GridBaselineAlignment::IsOrthogonalChildForBaseline(
    const LayoutBox& child) const {
  return IsHorizontalWritingMode(block_flow_) !=
         child.IsHorizontalWritingMode();
}

bool GridBaselineAlignment::IsParallelToBaselineAxisForChild(
    const LayoutBox& child,
    GridAxis axis) const {
  return axis == kGridColumnAxis ? !IsOrthogonalChildForBaseline(child)
                                 : IsOrthogonalChildForBaseline(child);
}

const BaselineGroup& GridBaselineAlignment::GetBaselineGroupForChild(
    ItemPosition preference,
    unsigned shared_context,
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  DCHECK(IsBaselinePosition(preference));
  bool is_row_axis_context = baseline_axis == kGridColumnAxis;
  auto& contexts_map = is_row_axis_context ? row_axis_alignment_context_
                                           : col_axis_alignment_context_;
  auto* context = contexts_map.at(shared_context);
  DCHECK(context);
  return context->GetSharedGroup(child, preference);
}

void GridBaselineAlignment::UpdateBaselineAlignmentContext(
    ItemPosition preference,
    unsigned shared_context,
    const LayoutBox& child,
    GridAxis baseline_axis) {
  DCHECK(IsBaselinePosition(preference));
  DCHECK(!child.NeedsLayout());

  // Determine Ascent and Descent values of this child with respect to
  // its grid container.
  LayoutUnit ascent = AscentForChild(child, baseline_axis);
  LayoutUnit descent = DescentForChild(child, ascent, baseline_axis);
  if (IsDescentBaselineForChild(child, baseline_axis))
    std::swap(ascent, descent);

  // Looking up for a shared alignment context perpendicular to the
  // baseline axis.
  bool is_row_axis_context = baseline_axis == kGridColumnAxis;
  auto& contexts_map = is_row_axis_context ? row_axis_alignment_context_
                                           : col_axis_alignment_context_;
  auto add_result = contexts_map.insert(shared_context, nullptr);

  // Looking for a compatible baseline-sharing group.
  if (add_result.is_new_entry) {
    add_result.stored_value->value =
        std::make_unique<BaselineContext>(child, preference, ascent, descent);
  } else {
    auto* context = add_result.stored_value->value.get();
    context->UpdateSharedGroup(child, preference, ascent, descent);
  }
}

LayoutUnit GridBaselineAlignment::BaselineOffsetForChild(
    ItemPosition preference,
    unsigned shared_context,
    const LayoutBox& child,
    GridAxis baseline_axis) const {
  DCHECK(IsBaselinePosition(preference));
  auto& group = GetBaselineGroupForChild(preference, shared_context, child,
                                         baseline_axis);
  if (group.size() > 1) {
    return group.MaxAscent() - LogicalAscentForChild(child, baseline_axis);
  }
  return LayoutUnit();
}

void GridBaselineAlignment::Clear(GridAxis baseline_axis) {
  if (baseline_axis == kGridColumnAxis)
    row_axis_alignment_context_.clear();
  else
    col_axis_alignment_context_.clear();
}

BaselineGroup::BaselineGroup(WritingMode block_flow,
                             ItemPosition child_preference)
    : max_ascent_(0), max_descent_(0), items_() {
  block_flow_ = block_flow;
  preference_ = child_preference;
}

void BaselineGroup::Update(const LayoutBox& child,
                           LayoutUnit ascent,
                           LayoutUnit descent) {
  if (items_.insert(&child).is_new_entry) {
    max_ascent_ = std::max(max_ascent_, ascent);
    max_descent_ = std::max(max_descent_, descent);
  }
}

bool BaselineGroup::IsOppositeBlockFlow(WritingMode block_flow) const {
  switch (block_flow) {
    case WritingMode::kHorizontalTb:
      return false;
    case WritingMode::kVerticalLr:
      return block_flow_ == WritingMode::kVerticalRl;
    case WritingMode::kVerticalRl:
      return block_flow_ == WritingMode::kVerticalLr;
    default:
      NOTREACHED();
      return false;
  }
}

bool BaselineGroup::IsOrthogonalBlockFlow(WritingMode block_flow) const {
  switch (block_flow) {
    case WritingMode::kHorizontalTb:
      return block_flow_ != WritingMode::kHorizontalTb;
    case WritingMode::kVerticalLr:
    case WritingMode::kVerticalRl:
      return block_flow_ == WritingMode::kHorizontalTb;
    default:
      NOTREACHED();
      return false;
  }
}

bool BaselineGroup::IsCompatible(WritingMode child_block_flow,
                                 ItemPosition child_preference) const {
  DCHECK(IsBaselinePosition(child_preference));
  DCHECK_GT(size(), 0);
  return ((block_flow_ == child_block_flow ||
           IsOrthogonalBlockFlow(child_block_flow)) &&
          preference_ == child_preference) ||
         (IsOppositeBlockFlow(child_block_flow) &&
          preference_ != child_preference);
}

BaselineContext::BaselineContext(const LayoutBox& child,
                                 ItemPosition preference,
                                 LayoutUnit ascent,
                                 LayoutUnit descent) {
  DCHECK(IsBaselinePosition(preference));
  UpdateSharedGroup(child, preference, ascent, descent);
}

const BaselineGroup& BaselineContext::GetSharedGroup(
    const LayoutBox& child,
    ItemPosition preference) const {
  DCHECK(IsBaselinePosition(preference));
  return const_cast<BaselineContext*>(this)->FindCompatibleSharedGroup(
      child, preference);
}

void BaselineContext::UpdateSharedGroup(const LayoutBox& child,
                                        ItemPosition preference,
                                        LayoutUnit ascent,
                                        LayoutUnit descent) {
  DCHECK(IsBaselinePosition(preference));
  BaselineGroup& group = FindCompatibleSharedGroup(child, preference);
  group.Update(child, ascent, descent);
}

// TODO Properly implement baseline-group compatibility
// See https://github.com/w3c/csswg-drafts/issues/721
BaselineGroup& BaselineContext::FindCompatibleSharedGroup(
    const LayoutBox& child,
    ItemPosition preference) {
  WritingMode block_direction = child.StyleRef().GetWritingMode();
  for (auto& group : shared_groups_) {
    if (group.IsCompatible(block_direction, preference))
      return group;
  }
  shared_groups_.push_front(BaselineGroup(block_direction, preference));
  return shared_groups_[0];
}

}  // namespace blink
