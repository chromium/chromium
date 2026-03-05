// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/layout/break_token_algorithm_data.h"
#include "third_party/blink/renderer/core/layout/flex/flex_line.h"

namespace blink {

struct FlexBreakTokenData final : BreakTokenAlgorithmData {
  // FlexBreakBeforeRow is used to maintain the state of break before rows
  // during flex fragmentation. kNotBreakBeforeRow implies that we are either
  // fragmenting a column-based flex container, or the current break token does
  // not represent a break before a row. If kAtStartOfBreakBeforeRow is set,
  // then the current break token represents a break before a row, and it is the
  // first time we broke before the given row. If kPastStartOfBreakBeforeRow is
  // set, then the current break token similarly represents a break before a
  // row, but it is not the first time we've broken before the given row.
  enum FlexBreakBeforeRow {
    kNotBreakBeforeRow,
    kAtStartOfBreakBeforeRow,
    kPastStartOfBreakBeforeRow
  };

  FlexBreakTokenData(const FlexLineVector& flex_lines,
                     const Vector<EBreakBetween>& row_break_between,
                     const HeapVector<Member<LayoutBox>>& oof_children,
                     LayoutUnit intrinsic_block_size,
                     FlexBreakBeforeRow break_before_row,
                     LayoutUnit effective_gap_between_lines)
      : BreakTokenAlgorithmData(kFlexData),
        flex_lines(flex_lines),
        row_break_between(row_break_between),
        oof_children(oof_children),
        intrinsic_block_size(intrinsic_block_size),
        break_before_row(break_before_row),
        effective_gap_between_lines(effective_gap_between_lines) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(flex_lines);
    visitor->Trace(oof_children);
    BreakTokenAlgorithmData::Trace(visitor);
  }

  FlexLineVector flex_lines;
  Vector<EBreakBetween> row_break_between;
  HeapVector<Member<LayoutBox>> oof_children;
  LayoutUnit intrinsic_block_size;
  // `break_before_row` is only used in the case of row flex containers. If this
  // is set to anything other than kNotBreakBeforeRow, that means that the next
  // row to be processed has broken before, as represented by a break before its
  // first child.
  //
  // We do not clamp row gaps, so we can have more than one break before a row.
  // There are certain adjustments we only want to make the first time a row
  // breaks before. Thus, we will also track if the current break before is the
  // first, or if we are past the first break before row (as distinguished by
  // the kAtStartOfBreakBeforeRow and kPastStartOfBreakBeforeRow values).
  FlexBreakBeforeRow break_before_row = kNotBreakBeforeRow;

  // The effective gap between lines includes both the base CSS gap
  // value and any additional spacing from content distribution (e.g.,
  // space-between, space-around). This is computed once in the first fragment
  // inside `GiveItemsFinalPositionAndSize()`.
  //
  // The effective gap between lines is used for gap decorations placement,
  // and for the purposes of gap decorations, it is the same across all
  // fragments, so the effective gap must be carried forward here for gap
  // decoration placement and gap suppression during fragmentation.
  LayoutUnit effective_gap_between_lines;
};

template <>
struct DowncastTraits<FlexBreakTokenData> {
  static bool AllowFrom(const BreakTokenAlgorithmData& token_data) {
    return token_data.IsFlexType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_BREAK_TOKEN_DATA_H_
