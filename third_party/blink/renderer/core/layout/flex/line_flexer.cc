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
#if DCHECK_IS_ON()
  // All items should be set to their hypothetical size initially.
  for (auto& item : line_items_) {
    DCHECK_EQ(item.flexed_content_size, item.hypothetical_content_size);
  }
#endif
  // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths
  // step 3, freeze any item with:
  //  - A flex-factor of 0.
  //  - Any with a min/max size violation.
  FreezeItems([mode = mode_](const FlexItem& item) {
    const float flex_factor =
        (mode == kGrow) ? item.flex_grow : item.flex_shrink;
    if (flex_factor == 0.f) {
      return true;
    }

    return mode == kGrow
               ? item.base_content_size > item.hypothetical_content_size
               : item.base_content_size < item.hypothetical_content_size;
  });

  initial_free_space_ = free_space_;
}

template <typename ShouldFreezeFunc>
void LineFlexer::FreezeItems(ShouldFreezeFunc should_freeze) {
  // Re-calculate the flex-factor sum, and free-space.
  //
  // NOTE: Because floating-point isn't associative we don't subtract the
  // flex-factors.
  // https://en.wikipedia.org/wiki/Associative_property#Nonassociativity_of_floating-point_calculation
  total_flex_factor_ = 0.0;
  double total_weighted_flex_shrink = 0.0;

  free_space_ = main_axis_inner_size_;
  for (auto& item : line_items_) {
    // Determine if we should freeze this item.
    item.state = (item.state == FlexerState::kFrozen || should_freeze(item))
                     ? FlexerState::kFrozen
                     : FlexerState::kNone;

    // If this item is frozen don't add to the flex-factor sums.
    if (item.state == FlexerState::kFrozen) {
      free_space_ -= item.FlexedMarginBoxSize() + gap_between_items_;
      continue;
    }

    // Reset the flexed_content_size to its initial state.
    item.flexed_content_size = item.hypothetical_content_size;

    const double flex_factor =
        (mode_ == kGrow) ? item.flex_grow : item.flex_shrink;
    DCHECK(flex_factor != 0.0);
    total_flex_factor_ += flex_factor;

    if (mode_ == kGrow) {
      item.free_space_fraction = flex_factor / total_flex_factor_;
    } else {
      const double weighted_flex_shrink = flex_factor * item.base_content_size;
      total_weighted_flex_shrink += weighted_flex_shrink;
      item.free_space_fraction =
          weighted_flex_shrink / total_weighted_flex_shrink;
    }

    free_space_ -= item.FlexBaseMarginBoxSize() + gap_between_items_;
  }
  free_space_ += gap_between_items_;
}

bool LineFlexer::ResolveFlexibleLengths() {
  // If the total flex-factors are less than one, we should only distribute
  // into the free-space limited by this factor. E.g. if we have:
  //
  // <div style="display: flex; width: 100px;">
  //   <div style="flex-grow: 0.5;"></div>
  // </div>
  //
  // The item should grow to 50px, not 100px.
  if (total_flex_factor_ > 0.0 && total_flex_factor_ < 1.0) {
    LayoutUnit fractional(initial_free_space_ * total_flex_factor_);
    if (fractional.Abs() < free_space_.Abs()) {
      free_space_ = fractional;
    }
  }

  // We can early exit if there isn't any free-space to distribute.
  if (mode_ == kGrow) {
    if (free_space_ <= LayoutUnit()) {
      return false;
    }
  } else {
    if (free_space_ >= LayoutUnit()) {
      return false;
    }
  }

  // How we distribute the free-space has hidden complexity.
  // Let's say we have three flex-items each with a flex-grow of 1.
  //
  // Naively you would distribute the free-space by multiplying the free-space
  // by 1/3 each time.
  //
  // However this can produce overflow/underflow resulting in unwanted
  // scrollbars, etc.
  //
  // Instead of this we calculate the *fraction* of the free-space each item
  // should receive, *after* the previous items have subtracted from this
  // free-space.
  //
  // Given the above three items, lets say we have 100px to distribute, and we
  // work in whole pixels.
  //  - The last item will have a fraction of (1/3) resulting in 33px of space,
  //    and 67px of free-space.
  //  - The next item will have a fraction of (1/2) resulting in 34px of space,
  //    and 33px of free-space.
  //  - The first item will have a fraction of (1/1) and receive the remaining
  //    33px of free-space.
  //
  // This method converges, and avoids rounding issues.
  LayoutUnit total_violation;
  for (auto& item : base::Reversed(line_items_)) {
    if (item.state == FlexerState::kFrozen) {
      continue;
    }

    const LayoutUnit extra_size = ([&]() {
      // Special case the basecase to avoid rounding issues.
      if (item.free_space_fraction == 1.0) {
        return free_space_;
      }

      const double extra = free_space_ * item.free_space_fraction;
      return std::isfinite(extra) ? LayoutUnit::FromDoubleRound(extra)
                                  : LayoutUnit();
    })();
    free_space_ -= extra_size;

    const LayoutUnit item_size = item.base_content_size + extra_size;

    const LayoutUnit adjusted_item_size =
        item.main_axis_min_max_sizes.ClampSizeToMinAndMax(item_size);
    DCHECK_GE(adjusted_item_size, LayoutUnit());
    item.flexed_content_size = adjusted_item_size;

    const LayoutUnit violation = adjusted_item_size - item_size;
    if (violation) {
      item.state = violation < LayoutUnit() ? FlexerState::kMaxViolation
                                            : FlexerState::kMinViolation;
    }
    total_violation += violation;
  }

  if (total_violation) {
    const FlexerState state = total_violation < LayoutUnit()
                                  ? FlexerState::kMaxViolation
                                  : FlexerState::kMinViolation;
    FreezeItems([state](const FlexItem& item) { return item.state == state; });
    return true;
  }

  return false;
}

}  // namespace blink
