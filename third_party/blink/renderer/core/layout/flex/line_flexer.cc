// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/line_flexer.h"

namespace blink {

LineFlexer::LineFlexer(base::span<FlexItem> line_items,
                       LayoutUnit main_axis_inner_size,
                       LayoutUnit sum_hypothetical_main_size,
                       LayoutUnit gap_between_items)
    : line_items_(line_items),
      main_axis_inner_size_(main_axis_inner_size),
      gap_between_items_(gap_between_items),
      mode_(sum_hypothetical_main_size < main_axis_inner_size ? kGrow
                                                              : kShrink) {
  LayoutUnit used_space;
  for (auto& item : line_items_) {
    // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths
    // step 3, freeze any item with:
    //  - A flex-factor of 0.
    //  - Any with a min/max size violation.
    const float flex_factor =
        (mode_ == kGrow) ? item.flex_grow : item.flex_shrink;
    if (flex_factor == 0.f ||
        (mode_ == kGrow &&
         item.base_content_size > item.hypothetical_content_size) ||
        (mode_ == kShrink &&
         item.base_content_size < item.hypothetical_content_size)) {
      item.flexed_content_size = item.hypothetical_content_size;
      item.state = FlexerState::kFrozen;
      used_space += item.FlexedMarginBoxSize() + gap_between_items_;
      continue;
    }

    total_flex_grow_ += item.flex_grow;
    total_flex_shrink_ += item.flex_shrink;
    total_weighted_flex_shrink_ += item.flex_shrink * item.base_content_size;
    used_space += item.FlexBaseMarginBoxSize() + gap_between_items_;
  }
  used_space -= gap_between_items_;

  remaining_free_space_ = main_axis_inner_size_ - used_space;
  initial_free_space_ = remaining_free_space_;
}

void LineFlexer::FreezeViolations(FlexerState should_freeze) {
  // Re-calculate the flex-factor sums, and free-space.
  //
  // NOTE: Because floating-point isn't associative we don't subtract the
  // flex-factors.
  // https://en.wikipedia.org/wiki/Associative_property#Nonassociativity_of_floating-point_calculation
  total_flex_grow_ = 0.0;
  total_flex_shrink_ = 0.0;
  total_weighted_flex_shrink_ = 0.0;

  LayoutUnit used_space;
  for (auto& item : line_items_) {
    // Determine if we should freeze this item.
    item.state =
        (item.state == FlexerState::kFrozen || item.state == should_freeze)
            ? FlexerState::kFrozen
            : FlexerState::kNone;

    // If this item is frozen don't add to the flex-factor sums.
    if (item.state == FlexerState::kFrozen) {
      used_space += item.FlexedMarginBoxSize() + gap_between_items_;
      continue;
    }

    total_flex_grow_ += item.flex_grow;
    total_flex_shrink_ += item.flex_shrink;
    total_weighted_flex_shrink_ += item.flex_shrink * item.base_content_size;
    used_space += item.FlexBaseMarginBoxSize() + gap_between_items_;
  }
  used_space -= gap_between_items_;

  remaining_free_space_ = main_axis_inner_size_ - used_space;
}

bool LineFlexer::ResolveFlexibleLengths() {
  const double sum_flex_factors =
      (mode_ == kGrow) ? total_flex_grow_ : total_flex_shrink_;
  if (sum_flex_factors > 0 && sum_flex_factors < 1) {
    LayoutUnit fractional(initial_free_space_ * sum_flex_factors);
    if (fractional.Abs() < remaining_free_space_.Abs()) {
      remaining_free_space_ = fractional;
    }
  }

  LayoutUnit total_violation;
  for (auto& item : line_items_) {
    if (item.state == FlexerState::kFrozen) {
      continue;
    }

    LayoutUnit child_size = item.base_content_size;
    double extra_space = 0;
    if (remaining_free_space_ > 0 && total_flex_grow_ > 0 && mode_ == kGrow &&
        std::isfinite(total_flex_grow_)) {
      extra_space = remaining_free_space_ * item.flex_grow / total_flex_grow_;
    } else if (remaining_free_space_ < 0 && total_weighted_flex_shrink_ > 0 &&
               mode_ == kShrink && std::isfinite(total_weighted_flex_shrink_) &&
               item.flex_shrink) {
      extra_space = remaining_free_space_ * item.flex_shrink *
                    item.base_content_size / total_weighted_flex_shrink_;
    }
    if (std::isfinite(extra_space)) {
      child_size += LayoutUnit::FromFloatRound(extra_space);
    }

    const LayoutUnit adjusted_child_size =
        item.main_axis_min_max_sizes.ClampSizeToMinAndMax(child_size);
    DCHECK_GE(adjusted_child_size, LayoutUnit());
    item.flexed_content_size = adjusted_child_size;

    const LayoutUnit violation = adjusted_child_size - child_size;
    if (violation > LayoutUnit()) {
      item.state = FlexerState::kMinViolation;
    } else if (violation < LayoutUnit()) {
      item.state = FlexerState::kMaxViolation;
    }
    total_violation += violation;
  }

  if (total_violation) {
    FreezeViolations(total_violation < LayoutUnit()
                         ? FlexerState::kMaxViolation
                         : FlexerState::kMinViolation);
    return true;
  }

  return false;
}

}  // namespace blink
