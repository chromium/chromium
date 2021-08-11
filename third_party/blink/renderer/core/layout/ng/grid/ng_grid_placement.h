// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PLACEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PLACEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/platform/wtf/doubly_linked_list.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This class encapsulates the Grid Item Placement Algorithm described by
// https://drafts.csswg.org/css-grid/#auto-placement-algo
class CORE_EXPORT NGGridPlacement {
  STACK_ALLOCATED();

 public:
  enum class PackingBehavior { kSparse, kDense };

  using GridItems = NGGridLayoutAlgorithm::GridItems;
  using GridItemData = NGGridLayoutAlgorithm::GridItemData;
  using GridItemVector = NGGridLayoutAlgorithm::GridItemVector;
  using AutoPlacementType = NGGridLayoutAlgorithm::AutoPlacementType;

  NGGridPlacement(const ComputedStyle& grid_style,
                  const wtf_size_t column_auto_repetitions,
                  const wtf_size_t row_auto_repetitions,
                  const wtf_size_t column_start_offset = 0,
                  const wtf_size_t row_start_offset = 0);

  void RunAutoPlacementAlgorithm(GridItems* grid_items);
  // Helper function to resolve start and end lines of out of flow items.
  void ResolveOutOfFlowItemGridLines(
      const NGGridLayoutAlgorithmTrackCollection& track_collection,
      const ComputedStyle& out_of_flow_item_style,
      wtf_size_t* start_line,
      wtf_size_t* end_line) const;

  wtf_size_t AutoRepeatTrackCount(
      const GridTrackSizingDirection track_direction) const;
  wtf_size_t AutoRepetitions(
      const GridTrackSizingDirection track_direction) const;
  wtf_size_t StartOffset(const GridTrackSizingDirection track_direction) const;

 private:
  enum class CursorMovementBehavior { kAuto, kForceMajorLine, kForceMinorLine };

  struct GridPosition {
    bool operator<=(const GridPosition& other) const;
    bool operator<(const GridPosition& other) const;

    wtf_size_t major_line{0};
    wtf_size_t minor_line{0};
  };

  class PlacedGridItem final : public DoublyLinkedListNode<PlacedGridItem> {
    USING_FAST_MALLOC(PlacedGridItem);
    friend class DoublyLinkedListNode<PlacedGridItem>;

   public:
    PlacedGridItem(const GridItemData& grid_item,
                   const GridTrackSizingDirection major_direction,
                   const GridTrackSizingDirection minor_direction);

    bool operator<(const PlacedGridItem& rhs) const {
      return start_ < rhs.start_;
    }

    GridPosition Start() const { return start_; }
    GridPosition End() const { return end_; }
    GridPosition EndOnPreviousMajorLine() const;

    wtf_size_t MajorStartLine() const { return start_.major_line; }
    wtf_size_t MinorStartLine() const { return start_.minor_line; }
    wtf_size_t MajorEndLine() const { return end_.major_line; }
    wtf_size_t MinorEndLine() const { return end_.minor_line; }

   private:
    GridPosition start_, end_;
    PlacedGridItem* next_{nullptr};
    PlacedGridItem* prev_{nullptr};
  };

  class AutoPlacementCursor {
   public:
    explicit AutoPlacementCursor(const PlacedGridItem* first_placed_item)
        : has_new_item_overlapping_major_line_(false),
          next_placed_item_(first_placed_item) {}

    void MoveCursorToFitGridSpan(
        const wtf_size_t major_span_size,
        const wtf_size_t minor_span_size,
        const wtf_size_t minor_max_end_line,
        const CursorMovementBehavior movement_behavior);
    void MoveToMajorLine(const wtf_size_t major_line);
    void MoveToMinorLine(const wtf_size_t minor_line);

    void InsertPlacedItemAtCurrentPosition(
        const PlacedGridItem* new_placed_item);

    wtf_size_t MajorLine() const { return current_position_.major_line; }
    wtf_size_t MinorLine() const { return current_position_.minor_line; }

    const PlacedGridItem* NextPlacedItem() const { return next_placed_item_; }

   private:
    // Comparer needed to use |items_overlapping_major_line_| as a heap.
    struct {
      bool operator()(const PlacedGridItem* lhs,
                      const PlacedGridItem* rhs) const {
        return rhs->End() < lhs->End();
      }
    } ComparePlacedGridItemsByEnd;

    void MoveToNextMajorLine(const bool allow_minor_line_movement);
    void UpdateItemsOverlappingMajorLine();

    Vector<const PlacedGridItem*, 16> items_overlapping_major_line_;
    bool has_new_item_overlapping_major_line_ : 1;
    const PlacedGridItem* next_placed_item_;
    GridPosition current_position_;
  };

  struct PlacedGridItemsList {
    void AppendCurrentItemsToOrderedList();
    PlacedGridItem* FirstPlacedItem() { return ordered_list.Head(); }

    Vector<std::unique_ptr<PlacedGridItem>, 16> item_vector;
    DoublyLinkedList<PlacedGridItem> ordered_list;
    bool needs_to_sort_item_vector : 1;
  };

  // Place non auto-positioned elements from |grid_items|; returns true if any
  // item needs to resolve an automatic position. Otherwise, false.
  bool PlaceNonAutoGridItems(GridItems* grid_items,
                             GridItemVector* items_locked_to_major_axis,
                             GridItemVector* items_not_locked_to_major_axis,
                             PlacedGridItemsList* placed_items);
  // Place elements from |grid_items| that have a definite position on the major
  // axis but need auto-placement on the minor axis.
  void PlaceGridItemsLockedToMajorAxis(
      const GridItemVector& items_locked_to_major_axis,
      PlacedGridItemsList* placed_items);
  // Place an item that has a definite position on the minor axis but need
  // auto-placement on the major axis.
  void PlaceAutoMajorAxisGridItem(GridItemData* grid_item,
                                  PlacedGridItemsList* placed_items,
                                  AutoPlacementCursor* placement_cursor) const;
  // Place an item that needs auto-placement on both the major and minor axis.
  void PlaceAutoBothAxisGridItem(GridItemData* grid_item,
                                 PlacedGridItemsList* placed_items,
                                 AutoPlacementCursor* placement_cursor) const;
  // Update the list of placed grid items and auto-placement cursor using the
  // resolved position of the specified grid item.
  void PlaceGridItemAtCursor(const GridItemData& grid_item,
                             PlacedGridItemsList* placed_items,
                             AutoPlacementCursor* placement_cursor) const;

  bool HasSparsePacking() const;

  // Used to resolve positions using |GridPositionsResolver|.
  const ComputedStyle& grid_style_;

  const PackingBehavior packing_behavior_;
  const GridTrackSizingDirection major_direction_;
  const GridTrackSizingDirection minor_direction_;
  const wtf_size_t column_auto_repeat_track_count_;
  const wtf_size_t row_auto_repeat_track_count_;
  const wtf_size_t column_auto_repetitions_;
  const wtf_size_t row_auto_repetitions_;

  wtf_size_t minor_max_end_line_;
  wtf_size_t column_start_offset_;
  wtf_size_t row_start_offset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_PLACEMENT_H_
