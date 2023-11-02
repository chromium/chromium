// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid_linked_list.h"
#include "third_party/blink/renderer/core/layout/order_iterator.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

namespace blink {

struct OrderedTrackIndexSetHashTraits : public HashTraits<wtf_size_t> {
  static const bool kEmptyValueIsZero = false;
  static wtf_size_t EmptyValue() { return UINT_MAX; }

  static void ConstructDeletedValue(wtf_size_t& slot, bool) {
    slot = UINT_MAX - 1;
  }
  static bool IsDeletedValue(const wtf_size_t& value) {
    return value == UINT_MAX - 1;
  }
};

typedef HeapVector<Member<LayoutBox>, 1> GridItemList;
typedef LinkedHashSet<wtf_size_t, OrderedTrackIndexSetHashTraits>
    OrderedTrackIndexSet;

class LayoutGrid;
class GridIterator;

// The Grid class represent a generic storage for grid items. This
// class is used by the LayoutGrid object to place the grid items on a
// grid like structure, so that they could be accessed by rows/columns
// instead of just traversing the DOM or Layout trees. The other user
// of this class is the GridTrackSizingAlgorithm class.
class CORE_EXPORT Grid : public GarbageCollected<Grid> {
 public:
  static Grid* Create(const LayoutGrid*);

  virtual wtf_size_t NumTracks(GridTrackSizingDirection) const = 0;

  virtual void EnsureGridSize(wtf_size_t maximum_row_size,
                              wtf_size_t maximum_column_size) = 0;
  virtual void Insert(LayoutBox&, const GridArea&) = 0;

  virtual const GridItemList& Cell(wtf_size_t row, wtf_size_t column) const = 0;

  virtual ~Grid() {}

  virtual void Trace(Visitor* visitor) const {
    visitor->Trace(order_iterator_);
    visitor->Trace(grid_item_area_);
    visitor->Trace(grid_items_indexes_map_);
  }

  // Note that out of flow children are not grid items.
  bool HasGridItems() const { return !grid_item_area_.empty(); }

  GridArea GridItemArea(const LayoutBox&) const;
  void SetGridItemArea(const LayoutBox&, GridArea);

  GridSpan GridItemSpan(const LayoutBox&, GridTrackSizingDirection) const;

  wtf_size_t GridItemPaintOrder(const LayoutBox&) const;
  void SetGridItemPaintOrder(const LayoutBox&, wtf_size_t order);

  wtf_size_t ExplicitGridStart(GridTrackSizingDirection) const;
  void SetExplicitGridStart(wtf_size_t row_start, wtf_size_t column_start);

  wtf_size_t AutoRepeatTracks(GridTrackSizingDirection) const;
  void SetAutoRepeatTracks(wtf_size_t auto_repeat_rows,
                           wtf_size_t auto_repeat_columns);

  void SetAutoRepeatEmptyColumns(std::unique_ptr<OrderedTrackIndexSet>);
  void SetAutoRepeatEmptyRows(std::unique_ptr<OrderedTrackIndexSet>);

  bool HasAutoRepeatEmptyTracks(GridTrackSizingDirection) const;
  bool IsEmptyAutoRepeatTrack(GridTrackSizingDirection, wtf_size_t) const;

  OrderedTrackIndexSet* AutoRepeatEmptyTracks(GridTrackSizingDirection) const;

  OrderIterator& GetOrderIterator() { return order_iterator_; }

  void SetNeedsItemsPlacement(bool);
  bool NeedsItemsPlacement() const { return needs_items_placement_; }

#if DCHECK_IS_ON()
  bool HasAnyGridItemPaintOrder() const;
#endif

  class GridIterator : public GarbageCollected<GridIterator> {
   public:
    virtual LayoutBox* NextGridItem() = 0;

    virtual std::unique_ptr<GridArea> NextEmptyGridArea(
        wtf_size_t fixed_track_span,
        wtf_size_t varying_track_span) = 0;

    GridIterator(const GridIterator&) = delete;
    GridIterator& operator=(const GridIterator&) = delete;
    virtual ~GridIterator() = default;

    virtual void Trace(Visitor* visitor) const {}

   protected:
    // |direction| is the direction that is fixed to |fixed_track_index| so e.g
    // GridIterator(grid_, kForColumns, 1) will walk over the rows of the 2nd
    // column.
    GridIterator(GridTrackSizingDirection,
                 wtf_size_t fixed_track_index,
                 wtf_size_t varying_track_index);

    GridTrackSizingDirection direction_;
    wtf_size_t row_index_;
    wtf_size_t column_index_;
    wtf_size_t child_index_;
  };

  virtual GridIterator* CreateIterator(
      GridTrackSizingDirection,
      wtf_size_t fixed_track_index,
      wtf_size_t varying_track_index = 0) const = 0;

 protected:
  Grid(const LayoutGrid*);

  virtual void ClearGridDataStructure() = 0;
  virtual void ConsolidateGridDataStructure() = 0;

 private:
  friend class GridIterator;

  OrderIterator order_iterator_;

  wtf_size_t explicit_column_start_{0};
  wtf_size_t explicit_row_start_{0};

  wtf_size_t auto_repeat_columns_{0};
  wtf_size_t auto_repeat_rows_{0};

  bool needs_items_placement_{true};

  HeapHashMap<Member<const LayoutBox>, GridArea> grid_item_area_;
  HeapHashMap<Member<const LayoutBox>, wtf_size_t> grid_items_indexes_map_;

  std::unique_ptr<OrderedTrackIndexSet> auto_repeat_empty_columns_{nullptr};
  std::unique_ptr<OrderedTrackIndexSet> auto_repeat_empty_rows_{nullptr};
};

// This is a Grid specialization which uses doubly linked lists (DLL)
// for the grid data structure. Each axis will be represented by a DLL
// of GridTrack's. The grid will only have list nodes for those tracks
// which actually contain at least one item. Those DLL are ordered by
// the track index.
class CORE_EXPORT ListGrid final : public Grid {
 public:
  explicit ListGrid(const LayoutGrid* grid)
      : Grid(grid),
        rows_(MakeGarbageCollected<GridLinkedList<GridTrack>>()),
        columns_(MakeGarbageCollected<GridLinkedList<GridTrack>>()) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(rows_);
    visitor->Trace(columns_);
    Grid::Trace(visitor);
  }

  wtf_size_t NumTracks(GridTrackSizingDirection direction) const override {
    return direction == kForRows ? num_rows_ : num_columns_;
  }
  const GridItemList& Cell(wtf_size_t row, wtf_size_t column) const override;
  void Insert(LayoutBox&, const GridArea&) override;
  void EnsureGridSize(wtf_size_t maximum_row_size,
                      wtf_size_t maximum_column_size) override;

  // This is the class representing a cell in the grid. GridCell's are
  // only created for those cells which do have items inside. Each
  // GridCell will be part of two different DLL, one representing the
  // column and another one representing the row.
  class GridCell final : public GridLinkedListNodeBase<GridCell> {
   public:
    GridCell(wtf_size_t row, wtf_size_t column) : row_(row), column_(column) {}

    wtf_size_t Index(GridTrackSizingDirection direction) const {
      return direction == kForRows ? row_ : column_;
    }

    void AppendItem(LayoutBox& item) { items_.push_back(&item); }

    const GridItemList& Items() const { return items_; }

    void Trace(Visitor* visitor) const final {
      visitor->Trace(prev_ortho_);
      visitor->Trace(next_ortho_);
      visitor->Trace(items_);
      GridLinkedListNodeBase<GridCell>::Trace(visitor);
    }

    // DoublyLinkedListNode classes must provide a next_ and prev_
    // pointers to the DoublyLinkedList class so that it could perform
    // the list operations. In the case of GridCells we need them to
    // be shared by two lists the row and the column. This means that
    // we need to maintain 4 separate pointers. In order to accomodate
    // this in the DoublyLinkedList model, we must set the proper
    // traversal mode (navigation by rows or columns) before any
    // operation with a GridCell involving the use of the next_/prev_
    // pointers.
    // TODO(svillar): we could probably use DoublyLinkedLists just for
    // one axis, this will remove the need for this and some other
    // clumsy things like different behaviours in ~GridTrack() for
    // each axis.
    void SetTraversalMode(GridTrackSizingDirection);
    GridTrackSizingDirection TraversalMode() const { return direction_; }

    // Use this ONLY for traversals. If your code performs any
    // modification in the list of cells while traversing then this
    // might not work as expected and you should use
    // SetTraversalMode()+Next() instead.
    GridCell* NextInDirection(GridTrackSizingDirection) const;

   private:
    Member<GridCell> prev_ortho_;
    Member<GridCell> next_ortho_;

    GridTrackSizingDirection direction_{kForColumns};
    GridItemList items_;
    wtf_size_t row_;
    wtf_size_t column_;
  };

  // This class represents a track (column or row) of the grid. Each
  // GridTrack will be part of a DLL stored in the ListGrid class,
  // either rows_ or columns_. GridTrack's are never empty, i.e., they
  // are only created whenever an item spans through them. Each
  // GridTrack keeps a sorted list of the cells containing grid items
  // in that particular track. The list of cells is ordered by the
  // index of the cell in the orthogonal direction, i.e., the list of
  // cells in a GridTrack representing a column will be sorted by
  // their row index.
  class CORE_EXPORT GridTrack final : public GridLinkedListNodeBase<GridTrack> {
   public:
    GridTrack(wtf_size_t index, GridTrackSizingDirection direction)
        : cells_(MakeGarbageCollected<GridLinkedList<GridCell>>()),
          index_(index),
          direction_(direction) {}

    wtf_size_t Index() const { return index_; }

    void Trace(Visitor* visitor) const final {
      visitor->Trace(cells_);
      GridLinkedListNodeBase<GridTrack>::Trace(visitor);
    }

    GridLinkedList<GridCell>::AddResult Insert(GridCell*);
    GridLinkedList<GridCell>::AddResult InsertAfter(GridCell* cell,
                                                    GridCell* insertion_point);
    GridLinkedList<GridCell>::AddResult Insert(LayoutBox&, const GridSpan&);
    GridCell* Find(wtf_size_t cell_index) const;

    const GridLinkedList<GridCell>& Cells() const { return *cells_; }

   private:
    Member<GridLinkedList<GridCell>> cells_;
    wtf_size_t index_;
    GridTrackSizingDirection direction_;
  };

 private:
  friend class ListGridIterator;

  // Returns a pointer to the first track.
  GridTrack* InsertTracks(GridLinkedList<GridTrack>&,
                          const GridSpan&,
                          GridTrackSizingDirection);

  void ClearGridDataStructure() override;
  void ConsolidateGridDataStructure() override {}

  const GridLinkedList<GridTrack>& Tracks(
      GridTrackSizingDirection direction) const {
    return direction == kForRows ? *rows_ : *columns_;
  }

  GridIterator* CreateIterator(
      GridTrackSizingDirection,
      wtf_size_t fixed_track_index,
      wtf_size_t varying_track_index = 0) const override;

  wtf_size_t num_rows_{0};
  wtf_size_t num_columns_{0};

  Member<GridLinkedList<GridTrack>> rows_;
  Member<GridLinkedList<GridTrack>> columns_;
};

class ListGridIterator final : public Grid::GridIterator {
 public:
  ListGridIterator(const ListGrid& grid,
                   GridTrackSizingDirection,
                   wtf_size_t fixed_track_index,
                   wtf_size_t varying_track_index = 0);
  ListGridIterator(const ListGridIterator&) = delete;
  ListGridIterator& operator=(const ListGridIterator&) = delete;

  void Trace(Visitor* visitor) const final {
    visitor->Trace(grid_);
    visitor->Trace(cell_node_);
    GridIterator::Trace(visitor);
  }

  LayoutBox* NextGridItem() override;
  std::unique_ptr<GridArea> NextEmptyGridArea(
      wtf_size_t fixed_track_span,
      wtf_size_t varying_track_span) override;

 private:
  Member<const ListGrid> grid_;
  Member<ListGrid::GridCell> cell_node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_
