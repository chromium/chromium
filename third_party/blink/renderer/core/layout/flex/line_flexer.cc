// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/line_flexer.h"

namespace blink {

LineFlexer::LineFlexer(base::span<FlexItem> line_items,
                       LayoutUnit sum_hypothetical_main_size,
                       LayoutUnit sum_flex_base_size,
                       LayoutUnit main_axis_inner_size)
    : line_items_(line_items),
      flex_sign_(sum_hypothetical_main_size < main_axis_inner_size
                     ? kPositive
                     : kNegative) {
  // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 2,
  // we freeze all items with a flex factor of 0 as well as those with a min/max
  // size violation.
  remaining_free_space_ = main_axis_inner_size - sum_flex_base_size;

  ViolationsVector new_inflexible_items;
  for (auto& flex_item : line_items_) {
    DCHECK(!flex_item.frozen);

    total_flex_grow_ += flex_item.flex_grow;
    total_flex_shrink_ += flex_item.flex_shrink;
    total_weighted_flex_shrink_ +=
        flex_item.flex_shrink * flex_item.base_content_size;

    float flex_factor =
        (flex_sign_ == kPositive) ? flex_item.flex_grow : flex_item.flex_shrink;
    if (flex_factor == 0 ||
        (flex_sign_ == kPositive &&
         flex_item.base_content_size > flex_item.hypothetical_content_size) ||
        (flex_sign_ == kNegative &&
         flex_item.base_content_size < flex_item.hypothetical_content_size)) {
      flex_item.flexed_content_size = flex_item.hypothetical_content_size;
      new_inflexible_items.push_back(&flex_item);
    }
  }
  FreezeViolations(new_inflexible_items);
  initial_free_space_ = remaining_free_space_;
}

void LineFlexer::FreezeViolations(ViolationsVector& violations) {
  for (auto* violation : violations) {
    DCHECK(!violation->frozen);
    remaining_free_space_ -=
        violation->flexed_content_size - violation->base_content_size;
    total_flex_grow_ -= violation->flex_grow;
    total_flex_shrink_ -= violation->flex_shrink;
    total_weighted_flex_shrink_ -=
        violation->flex_shrink * violation->base_content_size;
    // total_weighted_flex_shrink can be negative when we exceed the precision
    // of a double when we initially calculate total_weighted_flex_shrink. We
    // then subtract each child's weighted flex shrink with full precision, now
    // leading to a negative result. See
    // css3/flexbox/large-flex-shrink-assert.html
    total_weighted_flex_shrink_ = std::max(total_weighted_flex_shrink_, 0.0);
    violation->frozen = true;
  }
}

bool LineFlexer::ResolveFlexibleLengths() {
  LayoutUnit total_violation;
  LayoutUnit used_free_space;
  ViolationsVector min_violations;
  ViolationsVector max_violations;

  const double sum_flex_factors =
      (flex_sign_ == kPositive) ? total_flex_grow_ : total_flex_shrink_;
  if (sum_flex_factors > 0 && sum_flex_factors < 1) {
    LayoutUnit fractional(initial_free_space_ * sum_flex_factors);
    if (fractional.Abs() < remaining_free_space_.Abs()) {
      remaining_free_space_ = fractional;
    }
  }

  for (auto& flex_item : line_items_) {
    if (flex_item.frozen) {
      continue;
    }

    LayoutUnit child_size = flex_item.base_content_size;
    double extra_space = 0;
    if (remaining_free_space_ > 0 && total_flex_grow_ > 0 &&
        flex_sign_ == kPositive && std::isfinite(total_flex_grow_)) {
      extra_space =
          remaining_free_space_ * flex_item.flex_grow / total_flex_grow_;
    } else if (remaining_free_space_ < 0 && total_weighted_flex_shrink_ > 0 &&
               flex_sign_ == kNegative &&
               std::isfinite(total_weighted_flex_shrink_) &&
               flex_item.flex_shrink) {
      extra_space = remaining_free_space_ * flex_item.flex_shrink *
                    flex_item.base_content_size / total_weighted_flex_shrink_;
    }
    if (std::isfinite(extra_space)) {
      child_size += LayoutUnit::FromFloatRound(extra_space);
    }

    const LayoutUnit adjusted_child_size =
        flex_item.main_axis_min_max_sizes.ClampSizeToMinAndMax(child_size);
    DCHECK_GE(adjusted_child_size, 0);
    flex_item.flexed_content_size = adjusted_child_size;
    used_free_space += adjusted_child_size - flex_item.base_content_size;

    const LayoutUnit violation = adjusted_child_size - child_size;
    if (violation > 0) {
      min_violations.push_back(&flex_item);
    } else if (violation < 0) {
      max_violations.push_back(&flex_item);
    }
    total_violation += violation;
  }

  if (total_violation) {
    FreezeViolations(total_violation < 0 ? max_violations : min_violations);
  } else {
    remaining_free_space_ -= used_free_space;
  }

  return total_violation;
}

}  // namespace blink
