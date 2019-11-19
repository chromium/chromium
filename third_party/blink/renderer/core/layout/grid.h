// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/order_iterator.h"
#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/core/style/grid_positions_resolver.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/doubly_linked_list.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// TODO(svillar): Perhaps we should use references here.
typedef Vector<LayoutBox*, 1> GridItemList;
typedef LinkedHashSet<size_t> OrderedTrackIndexSet;

class LayoutGrid;
class GridIterator;

// The Grid class represent a generic storage for grid items. This
// class is used by the LayoutGrid object to place the grid items on a
// grid like structure, so that they could be accessed by rows/columns
// instead of just traversing the DOM or Layout trees. The other user
// of this class is the GridTrackSizingAlgorithm class.
class CORE_EXPORT Grid {
  USING_FAST_MALLOC(Grid);

 public:
  static std::unique_ptr<Grid> Create(const LayoutGrid*);

  virtual size_t NumTracks(GridTrackSizingDirection) const = 0;

  virtual void EnsureGridSize(size_t maximum_row_size,
                              size_t maximum_column_size) = 0;
  virtual void Insert(LayoutBox&, const GridArea&) = 0;

  virtual const GridItemList& Cell(size_t row, size_t column) const = 0;

  virtual ~Grid() {}

  // Note that out of flow children are not grid items.
  bool HasGridItems() const { return !grid_item_area_.IsEmpty(); }

  GridArea GridItemArea(const LayoutBox&) const;
  void SetGridItemArea(const LayoutBox&, GridArea);

  GridSpan GridItemSpan(const LayoutBox&, GridTrackSizingDirection) const;

  size_t GridItemPaintOrder(const LayoutBox&) const;
  void SetGridItemPaintOrder(const LayoutBox&, size_t order);

  int SmallestTrackStart(GridTrackSizingDirection) const;
  void SetSmallestTracksStart(int row_start, int column_start);

  size_t AutoRepeatTracks(GridTrackSizingDirection) const;
  void SetAutoRepeatTracks(size_t auto_repeat_rows, size_t auto_repeat_columns);

  typedef LinkedHashSet<size_t> OrderedTrackIndexSet;
  void SetAutoRepeatEmptyColumns(std::unique_ptr<OrderedTrackIndexSet>);
  void SetAutoRepeatEmptyRows(std::unique_ptr<OrderedTrackIndexSet>);

  bool HasAutoRepeatEmptyTracks(GridTrackSizingDirection) const;
  bool IsEmptyAutoRepeatTrack(GridTrackSizingDirection, size_t) const;

  OrderedTrackIndexSet* AutoRepeatEmptyTracks(GridTrackSizingDirection) const;

  OrderIterator& GetOrderIterator() { return order_iterator_; }

  void SetNeedsItemsPlacement(bool);
  bool NeedsItemsPlacement() const { return needs_items_placement_; }

#if DCHECK_IS_ON()
  bool HasAnyGridItemPaintOrder() const;
#endif

  class GridIterator {
    USING_FAST_MALLOC(GridIterator);

   public:
    virtual LayoutBox* NextGridItem() = 0;

    virtual std::unique_ptr<GridArea> NextEmptyGridArea(
        size_t fixed_track_span,
        size_t varying_track_span) = 0;

    virtual ~GridIterator() = default;

   protected:
    // |direction| is the direction that is fixed to |fixed_track_index| so e.g
    // GridIterator(grid_, kForColumns, 1) will walk over the rows of the 2nd
    // column.
    GridIterator(GridTrackSizingDirection,
                 size_t fixed_track_index,
                 size_t varying_track_index);

    GridTrackSizingDirection direction_;
    size_t row_index_;
    size_t column_index_;
    size_t child_index_;
    DISALLOW_COPY_AND_ASSIGN(GridIterator);
  };

  virtual std::unique_ptr<GridIterator> CreateIterator(
      GridTrackSizingDirection,
      size_t fixed_track_index,
      size_t varying_track_index = 0) const = 0;

 protected:
  Grid(const LayoutGrid*);

  virtual void ClearGridDataStructure() = 0;
  virtual void ConsolidateGridDataStructure() = 0;

 private:
  friend class GridIterator;

  OrderIterator order_iterator_;

  int smallest_column_start_{0};
  int smallest_row_start_{0};

  size_t auto_repeat_columns_{0};
  size_t auto_repeat_rows_{0};

  bool needs_items_placement_{true};

  HashMap<const LayoutBox*, GridArea> grid_item_area_;
  HashMap<const LayoutBox*, size_t> grid_items_indexes_map_;

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
  explicit ListGrid(const LayoutGrid* grid) : Grid(grid) {}

  size_t NumTracks(GridTrackSizingDirection direction) const override {
    return direction == kForRows ? num_rows_ : num_columns_;
  }
  const GridItemList& Cell(size_t row, size_t column) const override;
  void Insert(LayoutBox&, const GridArea&) override;
  void EnsureGridSize(size_t maximum_row_size,
                      size_t maximum_column_size) override;

  ~ListGrid() final;

  // This is the class representing a cell in the grid. GridCell's are
  // only created for those cells which do have items inside. Each
  // GridCell will be part of two different DLL, one representing the
  // column and another one representing the row.
  class GridCell final : public DoublyLinkedListNode<GridCell> {
    USING_FAST_MALLOC(GridCell);
    friend class WTF::DoublyLinkedListNode<GridCell>;

   public:
    GridCell(size_t row, size_t column) : row_(row), column_(column) {}

    size_t Index(GridTrackSizingDirection direction) const {
      return direction == kForRows ? row_ : column_;
    }

    void AppendItem(LayoutBox& item) { items_.push_back(&item); }

    const GridItemList& Items() const { return items_; }

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
    GridCell* prev_{nullptr};
    GridCell* next_{nullptr};
    GridCell* prev_ortho_{nullptr};
    GridCell* next_ortho_{nullptr};

    GridTrackSizingDirection direction_{kForColumns};
    GridItemList items_;
    size_t row_;
    size_t column_;
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
  class CORE_EXPORT GridTrack final : public DoublyLinkedListNode<GridTrack> {
    USING_FAST_MALLOC(GridTrack);
    friend class WTF::DoublyLinkedListNode<GridTrack>;

   public:
    GridTrack(size_t index, GridTrackSizingDirection direction)
        : index_(index), direction_(direction) {}

    size_t Index() const { return index_; }
    DoublyLinkedList<GridCell>::AddResult Insert(GridCell*);
    DoublyLinkedList<GridCell>::AddResult InsertAfter(
        GridCell* cell,
        GridCell* insertion_point);
    DoublyLinkedList<GridCell>::AddResult Insert(LayoutBox&, const GridSpan&);
    GridCell* Find(size_t cell_index) const;

    const DoublyLinkedList<GridCell>& Cells() const { return cells_; }

    ~GridTrack();

   private:
    DoublyLinkedList<GridCell> cells_;
    size_t index_;
    GridTrackSizingDirection direction_;

    GridTrack* prev_;
    GridTrack* next_;
  };

 private:
  friend class ListGridIterator;

  // Returns a pointer to the first track.
  GridTrack* InsertTracks(DoublyLinkedList<GridTrack>&,
                          const GridSpan&,
                          GridTrackSizingDirection);

  void ClearGridDataStructure() override;
  void ConsolidateGridDataStructure() override {}

  const DoublyLinkedList<GridTrack>& Tracks(
      GridTrackSizingDirection direction) const {
    return direction == kForRows ? rows_ : columns_;
  }

  std::unique_ptr<GridIterator> CreateIterator(
      GridTrackSizingDirection,
      size_t fixed_track_index,
      size_t varying_track_index = 0) const override;

  size_t num_rows_{0};
  size_t num_columns_{0};

  DoublyLinkedList<GridTrack> columns_;
  DoublyLinkedList<GridTrack> rows_;
};

class ListGridIterator final : public Grid::GridIterator {
  USING_FAST_MALLOC(ListGridIterator);

 public:
  ListGridIterator(const ListGrid& grid,
                   GridTrackSizingDirection,
                   size_t fixed_track_index,
                   size_t varying_track_index = 0);

  LayoutBox* NextGridItem() override;
  std::unique_ptr<GridArea> NextEmptyGridArea(
      size_t fixed_track_span,
      size_t varying_track_span) override;

 private:
  const ListGrid& grid_;
  ListGrid::GridCell* cell_node_{nullptr};
  DISALLOW_COPY_AND_ASSIGN(ListGridIterator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_H_
