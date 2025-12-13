// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_LINE_FLEXER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_LINE_FLEXER_H_

#include "third_party/blink/renderer/core/layout/flex/flex_item.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Takes a span of FlexItems and resolves their main-axis content-size based on
// their flex-grow/flex-shrink properties.
class LineFlexer {
  STACK_ALLOCATED();

 public:
  LineFlexer(base::span<FlexItem> line_items,
             LayoutUnit main_axis_inner_size,
             LayoutUnit sum_hypothetical_main_size,
             LayoutUnit gap_between_items);

  void Run() {
    while (ResolveFlexibleLengths()) {
      continue;
    }
  }

 private:
  enum FlexerMode {
    kGrow,
    kShrink,
  };

  template <typename ShouldFreezeFunc>
  void FreezeItems(ShouldFreezeFunc should_freeze);

  bool ResolveFlexibleLengths();

  base::span<FlexItem> line_items_;
  const LayoutUnit main_axis_inner_size_;
  const LayoutUnit gap_between_items_;
  const FlexerMode mode_;

  double total_flex_factor_ = 0.0;

  LayoutUnit initial_free_space_;
  LayoutUnit free_space_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_LINE_FLEXER_H_
