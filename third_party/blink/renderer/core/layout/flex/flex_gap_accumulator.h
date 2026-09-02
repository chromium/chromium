// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/flex/flex_break_token_data.h"
#include "third_party/blink/renderer/core/layout/flex/flex_line.h"
#include "third_party/blink/renderer/core/layout/gap/gap_geometry.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class BoxFragmentBuilder;
struct LogicalOffset;
struct FlexLine;

// We build and populate the gap intersections within the flex container in an
// item by item basis. The intersections that correspond to each item are
// defined as follows:
// 1. For the first item in a line, the intersections corresponding to it will
// be:
//  - The main axis (or row) intersection (X1) of the main axis gap after the
//  item's line, with the beginning of the flex line.
// +---------------------------------------------------------------+
// | +---------+        Gap        +---------+                     |
// | |  Item   |                   |         |                     |
// | +---------+                   +---------+                     |
// |                                                               |
// X1         Row Gap                                              |
// |                                                               |
// | +---------+        Gap        +---------+                     |
// | |         |                   |         |                     |
// | +---------+                   +---------+                     |
// +---------------------------------------------------------------+
// 2. For an item in the first line (and not the first item), the
// intersections corresponding to it will be:
//  - The cross axis intersection of the cross gap before the item, with the
//  edge of the flex line (X1).
//  - The main axis intersection of the cross gap with the main gap after the
//  item's line (X2)
//  - The cross axis intersection of the cross gap with the main gap after the
//  item's line (X2).
// +-----------------------X1--------------------------------------+
// | +---------+        Gap        +---------+                     |
// | |         |                   |  Item   |           ...       |
// | +---------+                   +---------+                     |
// |                                                               |
// |         Row Gap      X2                                       |
// |                                                               |
// | +---------+        Gap        +---------+                     |
// | |         |                   |         |                     |
// | +---------+                   +---------+                     |
// +---------------------------------------------------------------+
// 3. For the last item in any line, the intersections corresponding to it
// will be:
//  - The main axis intersection of the main axis gap after the item with the
//  edge of the flex line (X1).
// +--------------------------------------------------+
// | +---------+        Gap        +---------+        |
// | |         |                   |  Item   |        |
// | +---------+                   +---------+        |
// |                                                  |
// |         Row Gap                                  X1
// |    ...                              ...          |
// +---------------------------------------------------+
// 4. For items that lie in "middle" flex lines such as
//  `Item` in the example below, the intersections corresponding to it will
//  be:
//  - The main axis intersection of the cross gap before the item with the
//  main gap before the item's line (X1).
//  - The cross axis intersection of the cross gap before the item with the
//  main gap before the item's line (X1).
//  - The cross axis intersection of the cross gap before the item with the
//  main gap after the item's line (X2).
//  - The main axis intersection of the cross gap before the item with the
//  main gap after the item's line (X2).
// +----------------------------------------------------------------------+
// |        +---------+        Gap        +---------+                     |
// |   ...  |         |                   |         |          ...        |
// |        +---------+                   +---------+                     |
// |                                                                      |
// |                Row Gap     X1                                        |
// |                                                                      |
// |        +---------+        Gap        +---------+                     |
// |   ...  |         |                   |  Item   |          ...        |
// |        +---------+                   +---------+                     |
// |            .                             .                           |
// |            .   Row Gap     X2            .                           |
// |            .                             .                           |
// |            .                             .                           |
// +----------------------------------------------------------------------+
// 2. For an item (not the first or last) in the last line, the intersections
// corresponding to it will be:
//  - The cross (or column) intersection of the cross axis gap before the
//  item, with the main axis gap before the item's line (X1).
//  - The main (or row) intersection of the cross axis gap before the item,
//  with the main axis gap before the item's line (X1).
//  - The cross axis intersection of the cross gap before the item, with the
//  edge of the flex line (X2).
// +---------------------------------------------------------------+
// | +---------+        Gap        +---------+                     |
// | |         |                   |         |                     |
// | +---------+                   +---------+                     |
// |                                                               |
// |         Row Gap     X1                                        |
// |                                                               |
// | +---------+        Gap        +---------+                     |
// | |         |                   |  Item   |                     |
// | +---------+                   +---------+                     |
// +---------------------X2----------------------------------------+
// More information on gap intersections can be found in the spec:
// https://drafts.csswg.org/css-gaps-1/#layout-painting
//
// Important to note that all of this is fragment-relative. If the flexbox is
// fragmented, each fragment will have its own `GapGeometry`.
//
// TODO(javiercon): Consider refactoring this code to be able to be reused for
// grid-lanes, by abstracting away the flex-specific logic.
class CORE_EXPORT FlexGapAccumulator {
  STACK_ALLOCATED();

 public:
  explicit FlexGapAccumulator(
      LayoutUnit gap_between_items,
      LayoutUnit effective_gap_between_lines,
      wtf_size_t num_lines,
      wtf_size_t num_flex_items,
      bool is_column,
      LayoutUnit border_scrollbar_padding_block_start,
      LayoutUnit border_scrollbar_padding_inline_start,
      std::optional<GapGeometry::PlacementReversal> placement_reversal);

  const GapGeometry* BuildGapGeometry(
      const BoxFragmentBuilder& container_builder);

  // Creates `MainGaps` for fragmented column flexbox containers, since all
  // columns exist in every fragment.
  void InitializeFragmentedColumnGapGeometry(const FlexLineVector& flex_lines);

  // We populate the gap data structures within the flex container in an
  // item by item basis. The main and cross gaps that correspond to each item
  // are defined as follows:
  // 1. For the first item in a line, the `MainGap` corresponding to it will
  // be:
  //  - The main axis (or row) offset (X1) of the main axis gap after the
  //  item's line, with the beginning of the flex line.
  // +---------------------------------------------------------------+
  // | +---------+        Gap        +---------+                     |
  // | |  Item   |                   |         |                     |
  // | +---------+                   +---------+                     |
  // |                                                               |
  // X1         Row Gap                                              |
  // |                                                               |
  // | +---------+        Gap        +---------+                     |
  // | |         |                   |         |                     |
  // | +---------+                   +---------+                     |
  // +---------------------------------------------------------------+
  // 2. For an item in the first line (and not the first item), the
  // `CrossGap` corresponding to it will be:
  //  - The cross offset of the intersection point formed by the cross gap
  //  before the item, with the edge of the flex line (X1).
  // +-----------------------X1--------------------------------------+
  // | +---------+        Gap        +---------+                     |
  // | |         |                   |  Item   |           ...       |
  // | +---------+                   +---------+                     |
  // |                                                               |
  // |         Row Gap                                               |
  // |                                                               |
  // | +---------+        Gap        +---------+                     |
  // | |         |                   |         |                     |
  // | +---------+                   +---------+                     |
  // +---------------------------------------------------------------+
  // 4. For any items (`Item` in this example) that lie in all other positions,
  // the `CrossGap` corresponding to it will be:
  //  - The cross offset of the intersection point formed by the cross gap
  //  before the item with the main gap before the item's line (X1).
  // +----------------------------------------------------------------------+
  // |        +---------+        Gap        +---------+                     |
  // |   ...  |         |                   |         |          ...        |
  // |        +---------+                   +---------+                     |
  // |                                                                      |
  // |                Row Gap     X1                                        |
  // |                                                                      |
  // |        +---------+        Gap        +---------+                     |
  // |   ...  |         |                   |  Item   |          ...        |
  // |        +---------+                   +---------+                     |
  // |            .                             .                           |
  // |            .   Row Gap                   .                           |
  // |            .                             .                           |
  // |            .                             .                           |
  // +----------------------------------------------------------------------+
  //
  // For more information on GapDecorations implementation see
  // `third_party/blink/renderer/core/layout/gap/README.md`.
  void BuildGapsForCurrentItem(const FlexLineVector& flex_lines,
                               wtf_size_t global_line_index,
                               wtf_size_t item_index_in_line,
                               LogicalOffset item_offset,
                               bool is_first_item,
                               bool is_last_item,
                               bool is_last_line,
                               LayoutUnit line_cross_start,
                               LayoutUnit line_cross_end,
                               LayoutUnit container_main_end,
                               bool in_fragmentation = false);

  // Returns this fragment's row gap info: one entry for row flex (the main
  // gaps), and one entry per absolute flex line for column flex.
  Vector<FlexRowGapBreakTokenData> FinalizeRowGapBreakTokenData();

  void PopulateMainGapForFirstItem(LayoutUnit cross_end);

  // An absolute flex-line index indexes `flex_lines`. A geometry line index
  // indexes `GapGeometry`. It is an absolute flex-line index for column flex
  // and fragment-relative for row flex.
  void HandleCrossGapRangesForCurrentItem(
      wtf_size_t fragment_relative_line_index,
      wtf_size_t cross_gap_index);

  void PopulateCrossGapForCurrentItem(const FlexLine& flex_line,
                                      wtf_size_t global_line_index,
                                      wtf_size_t fragment_relative_line_index,
                                      bool is_first_line,
                                      bool is_last_line,
                                      bool single_line,
                                      LayoutUnit main_intersection_offset,
                                      LayoutUnit cross_start);

  // Calculates a column flex container line's `first_row_gap_index`. This is
  // done the moment we encounter a new line during item placement.
  // `previous_gap_data` is the prior fragment's flex gap break-token data, or
  // null when there is no previous fragment.
  void CalculateColumnFlexLineRowGapStart(
      const FlexLineVector& flex_lines,
      wtf_size_t global_line_index,
      const FlexGapBreakTokenData* previous_gap_data = nullptr);

  void SetContentMainEnd(LayoutUnit content_main_end) {
    content_main_end_ = content_main_end;
  }

  void SetEffectiveGapBetweenLines(LayoutUnit effective_gap) {
    effective_gap_between_lines_ = effective_gap;
  }

  // Increases the fragment-relative `row_gap_count` for the entry at
  // `row_gap_data_index`. This counts only the row gaps that land in the
  // current fragment (not the container's unfragmented total). Row flex passes
  // 0 (a single entry per fragment) while column flex passes the absolute flex
  // line index.
  void IncrementRowGapCount(wtf_size_t row_gap_data_index);

  // Decreases the current fragment's single row-flex gap count.
  void DecrementRowGapCount();

  // Removes the last `MainGap` from this fragment.
  void SuppressLastMainGap(
      std::optional<LayoutUnit> new_cross_end = std::nullopt);

 private:
  // Stores the index used to select the color, style, and width for the
  // `CrossGap` just added. This is needed because its fragment-local index may
  // differ from its index in the unfragmented flexbox.
  void RecordFragmentedFlexCrossGapDecorationIndex(
      const FlexLineVector& flex_lines,
      wtf_size_t global_line_index,
      wtf_size_t item_index_in_line);

  // This must be done after we are done laying out, so that we know the final
  // block size of the fragment. This only needs to be done for column
  // flexboxes, since the main end in such cases will be the final block end
  // of the fragment, which we will not know until  we are done laying out.
  void FinalizeContentMainEndForColumnFlex(
      const BoxFragmentBuilder& container_builder);

  void SetContentStartOffsetsIfNeeded(LogicalOffset offset,
                                      LayoutUnit line_cross_start);

  LayoutUnit gap_between_items_;

  // The effective gap between lines includes the base `gap` property plus any
  // additional space from cross-axis content distribution (e.g., space-between,
  // stretch).
  LayoutUnit effective_gap_between_lines_;

  bool is_column_ = false;

  GapGeometry* gap_geometry_ = nullptr;

  LayoutUnit border_scrollbar_padding_block_start_;
  LayoutUnit border_scrollbar_padding_inline_start_;

  LayoutUnit content_cross_start_ = LayoutUnit::Max();
  LayoutUnit content_cross_end_;
  LayoutUnit content_main_start_ = LayoutUnit::Max();

  LayoutUnit content_main_end_;

  // Tracks the index of the first row-flex line processed within the current
  // fragment.
  wtf_size_t first_row_flex_line_index_ = kNotFound;

  // The current fragment's row gap info, built during placement. Column flex
  // containers store one entry per absolute flex line while row flex
  // containers store a single entry for the whole fragment. This is because
  // row gaps exist per line in a column flex container while in the case of a
  // row flex container, row gaps separate flex lines in a given fragment.
  Vector<FlexRowGapBreakTokenData> row_gap_break_token_data_;

  // For every flex line, stores its first `CrossGap` index and number of
  // `CrossGap`s in the full flexbox.
  Vector<GapGeometry::GapIndexRange> fragmented_flex_line_gap_ranges_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_
