// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_LINE_BREAKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_LINE_BREAKER_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/layout/flex/flex_item.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Represents the items within a flex-line after line-breaking has occurred.
struct InitialFlexLine {
  DISALLOW_NEW();

 public:
  InitialFlexLine(wtf_size_t count,
                  LayoutUnit sum_hypothetical_main_size)
      : count(count),
        sum_hypothetical_main_size(sum_hypothetical_main_size) {}

  const wtf_size_t count;
  const LayoutUnit sum_hypothetical_main_size;
};

// The result of running the flex line-breaker.
struct FlexLineBreakerResult {
  STACK_ALLOCATED();

 public:
  ~FlexLineBreakerResult() { flex_lines.clear(); }

  HeapVector<InitialFlexLine, 1> flex_lines;
  LayoutUnit max_sum_hypothetical_main_size;
};

FlexLineBreakerResult BalanceBreakFlexItemsIntoLines(
    base::span<FlexItem> all_items,
    LayoutUnit line_break_size,
    LayoutUnit gap_between_items,
    wtf_size_t min_line_count);
FlexLineBreakerResult GreedyBreakFlexItemsIntoLines(
    base::span<FlexItem> all_items,
    LayoutUnit line_break_size,
    LayoutUnit gap_between_items,
    bool is_multi_line);

inline FlexLineBreakerResult BreakFlexItemsIntoLines(
    base::span<FlexItem> all_items,
    LayoutUnit line_break_size,
    LayoutUnit gap_between_items,
    bool is_multi_line,
    std::optional<wtf_size_t> balance_min_line_count) {
  if (all_items.empty()) {
    return FlexLineBreakerResult();
  }

  if (balance_min_line_count) {
    return BalanceBreakFlexItemsIntoLines(
        all_items, line_break_size, gap_between_items, *balance_min_line_count);
  }

  return GreedyBreakFlexItemsIntoLines(all_items, line_break_size,
                                       gap_between_items, is_multi_line);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_LINE_BREAKER_H_
