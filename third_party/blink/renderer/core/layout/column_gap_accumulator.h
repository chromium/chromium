// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_GAP_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_GAP_ACCUMULATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/gap/cross_gap.h"
#include "third_party/blink/renderer/core/layout/gap/gap_utils.h"
#include "third_party/blink/renderer/core/layout/gap/main_gap.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class BoxFragmentBuilder;
class GapGeometry;

// Accumulates gap-decoration state (main gaps, cross gaps, segment metadata)
// for a multicol container. Instantiated only when CSS gap decorations are
// enabled and the style has a gap rule. Mirrors FlexGapAccumulator and the
// grid-internal GapAccumulator in spirit.
class CORE_EXPORT ColumnGapAccumulator {
  STACK_ALLOCATED();

 public:
  ColumnGapAccumulator(LayoutUnit column_gap_size,
                       LayoutUnit row_gap_size,
                       wtf_size_t specified_column_count,
                       bool has_auto_column_count);

  void AddMainGap(LayoutUnit block_offset,
                  SpannerMainGapType gap_type = SpannerMainGapType::kNone);

  void AddEndSpannerMainGapIfNeeded(LayoutUnit block_offset);

  void AddStartSpannerMainGapIfNeeded(LayoutUnit block_offset);

  void AddCrossGap(LayoutUnit column_inline_start_offset);

  void AddNumberOfColumnsForCurrentRow(wtf_size_t cols_in_row);

  void SetFirstColumnOffsetIfNeeded(LogicalOffset offset);

  void UpdateMaxColumnsInRow(wtf_size_t count);

  bool ShouldAddCrossGapAt(wtf_size_t column_index_in_row) const;

  const GapGeometry* BuildGapGeometry(
      const BoxFragmentBuilder& container_builder,
      LayoutUnit column_inline_size);

 private:
  void FinalizeMainGapSegmentStateForCurrentRow(wtf_size_t cols_in_row);
  void UpdateCrossGapSegmentStates();
  bool LastMainGapIsStartSpanner() const;

  LayoutUnit column_gap_size_;
  LayoutUnit row_gap_size_;
  wtf_size_t specified_column_count_;
  bool has_auto_column_count_;

  Vector<MainGap> main_gaps_;
  Vector<CrossGap> cross_gaps_;
  std::optional<LogicalOffset> first_column_offset_;
  wtf_size_t max_columns_in_row_ = 0;
  std::optional<Vector<wtf_size_t>> columns_per_row_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_COLUMN_GAP_ACCUMULATOR_H_
