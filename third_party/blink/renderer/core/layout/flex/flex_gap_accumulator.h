// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class BoxFragmentBuilder;
class MainGap;
class CrossGap;
class GapGeometry;
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
 public:
  explicit FlexGapAccumulator(LayoutUnit gap_between_items,
                              LayoutUnit gap_between_lines,
                              wtf_size_t num_lines,
                              wtf_size_t num_flex_items,
                              bool is_column,
                              LayoutUnit border_scrollbar_padding_block_start,
                              LayoutUnit border_scrollbar_padding_inline_start)
      : gap_between_items_(gap_between_items),
        gap_between_lines_(gap_between_lines),
        is_column_(is_column),
        border_scrollbar_padding_block_start_(
            border_scrollbar_padding_block_start),
        border_scrollbar_padding_inline_start_(
            border_scrollbar_padding_inline_start) {
    cross_gaps_.ReserveInitialCapacity(num_flex_items);
    main_gaps_.ReserveInitialCapacity(num_lines - 1);
  }

  const GapGeometry* BuildGapGeometry(
      const BoxFragmentBuilder& container_builder);

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
  void BuildGapsForCurrentItem(const FlexLine& flex_line,
                               wtf_size_t flex_line_index,
                               LogicalOffset item_offset,
                               bool is_first_item,
                               bool is_last_item,
                               bool is_last_line,
                               LayoutUnit line_cross_start,
                               LayoutUnit line_cross_end,
                               LayoutUnit container_main_end);

  void PopulateMainGapForFirstItem(LayoutUnit cross_end);

  void HandleCrossGapRangesForCurrentItem(wtf_size_t flex_line_index,
                                          wtf_size_t cross_gap_index);

  void PopulateCrossGapForCurrentItem(const FlexLine& flex_line,
                                      wtf_size_t flex_line_index,
                                      bool is_first_line,
                                      bool is_last_line,
                                      bool single_line,
                                      LayoutUnit main_intersection_offset,
                                      LayoutUnit cross_start);

  void SetContentMainEnd(LayoutUnit content_main_end) {
    content_main_end_ = content_main_end;
  }

  const Vector<MainGap>& MainGaps() const { return main_gaps_; }

  // In the flex algorithm, there are some cases where we need to suppress a row
  // gap (i.e. if a row gap is the last content in a fragment). In such cases,
  // we must then also remove the `MainGap` that was created for that row gap
  // that will now be suppressed.
  void SuppressLastMainGap(
      std::optional<LayoutUnit> new_cross_end = std::nullopt);

 private:
  // This must be done after we are done laying out, so that we know the final
  // block size of the fragment. This only needs to be done for column
  // flexboxes, since the main end in such cases will be the final block end
  // of the fragment, which we will not know until  we are done laying out.
  void FinalizeContentMainEndForColumnFlex(
      const BoxFragmentBuilder& container_builder);

  void SetContentStartOffsetsIfNeeded(LogicalOffset offset,
                                      LayoutUnit line_cross_start);

  LayoutUnit gap_between_items_;
  LayoutUnit gap_between_lines_;
  bool is_column_ = false;

  Vector<MainGap> main_gaps_;
  Vector<CrossGap> cross_gaps_;

  LayoutUnit border_scrollbar_padding_block_start_;
  LayoutUnit border_scrollbar_padding_inline_start_;

  LayoutUnit content_cross_start_ = LayoutUnit::Max();
  LayoutUnit content_cross_end_;
  LayoutUnit content_main_start_ = LayoutUnit::Max();
  LayoutUnit content_main_end_;

  // Tracks the index of the first flex line procesesed within the current
  // fragment.
  wtf_size_t first_flex_line_processed_index_ = kNotFound;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_
