// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/flex/flex_line.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class MainGap;
class CrossGap;
class GapGeometry;
class BoxFragmentBuilder;
struct LogicalOffset;

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
// masonry, by abstracting away the flex-specific logic.
class CORE_EXPORT FlexGapAccumulator {
  STACK_ALLOCATED();

 public:
  explicit FlexGapAccumulator(LayoutUnit gap_between_items,
                              LayoutUnit gap_between_lines,
                              wtf_size_t num_lines,
                              wtf_size_t num_flex_items,
                              const BoxFragmentBuilder* container_builder,
                              bool is_column)
      : gap_between_items_(gap_between_items),
        gap_between_lines_(gap_between_lines),
        container_builder_(container_builder),
        is_column_(is_column) {
    CHECK(container_builder_);

    cross_gaps_.ReserveInitialCapacity(num_flex_items);
    main_gaps_.ReserveInitialCapacity(num_lines - 1);
  }

  const GapGeometry* BuildGapGeometry();

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
                               wtf_size_t flex_line_index,
                               wtf_size_t item_index_in_line,
                               LogicalOffset item_offset,
                               bool is_first_line,
                               bool is_last_line,
                               LayoutUnit line_cross_start,
                               LayoutUnit line_cross_end);

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

 private:
  LayoutUnit gap_between_items_;
  LayoutUnit gap_between_lines_;
  const BoxFragmentBuilder* container_builder_ = nullptr;
  bool is_column_ = false;

  Vector<MainGap> main_gaps_;
  Vector<CrossGap> cross_gaps_;

  LayoutUnit content_cross_start_;
  LayoutUnit content_cross_end_;
  LayoutUnit content_main_start_;
  LayoutUnit content_main_end_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FLEX_FLEX_GAP_ACCUMULATOR_H_
