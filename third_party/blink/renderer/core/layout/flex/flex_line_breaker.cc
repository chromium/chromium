// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/flex/flex_line_breaker.h"

namespace blink {

FlexLineBreakerResult BreakFlexItemsIntoLines(
    base::span<FlexItem> all_items,
    const LayoutUnit line_break_size,
    const LayoutUnit gap_between_items,
    const bool is_multi_line) {
  HeapVector<InitialFlexLine, 1> flex_lines;
  LayoutUnit max_sum_hypothetical_main_size;

  base::span<FlexItem> items = all_items;

  while (!items.empty()) {
    LayoutUnit sum_flex_base_size;
    LayoutUnit sum_hypothetical_main_size;
    wtf_size_t count = 0u;

    for (auto& item : items) {
      if (is_multi_line && count &&
          sum_hypothetical_main_size +
                  item.HypotheticalMainAxisMarginBoxSize() >
              line_break_size) {
        break;
      }

      sum_flex_base_size += item.FlexBaseMarginBoxSize() + gap_between_items;
      sum_hypothetical_main_size +=
          item.HypotheticalMainAxisMarginBoxSize() + gap_between_items;
      ++count;
    }
    // Take off the last gap (note we *always* have an item in the line).
    sum_hypothetical_main_size -= gap_between_items;
    sum_flex_base_size -= gap_between_items;

    auto [line_items, remaining_items] = items.split_at(count);
    flex_lines.emplace_back(line_items, sum_flex_base_size,
                            sum_hypothetical_main_size);
    max_sum_hypothetical_main_size =
        std::max(max_sum_hypothetical_main_size, sum_hypothetical_main_size);
    items = remaining_items;
  }

  return {flex_lines, max_sum_hypothetical_main_size};
}

}  // namespace blink
