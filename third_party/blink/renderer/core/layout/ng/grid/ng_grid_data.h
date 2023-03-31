// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_line_resolver.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_subtree.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct CORE_EXPORT NGGridPlacementData {
  USING_FAST_MALLOC(NGGridPlacementData);

 public:
  NGGridPlacementData(NGGridPlacementData&&) = default;
  NGGridPlacementData& operator=(NGGridPlacementData&&) = default;

  explicit NGGridPlacementData(const ComputedStyle& grid_style,
                               wtf_size_t column_auto_repetitions,
                               wtf_size_t row_auto_repetitions)
      : line_resolver(grid_style,
                      column_auto_repetitions,
                      row_auto_repetitions) {}

  // Subgrids need to map named lines from every parent grid. This constructor
  // should be used exclusively by subgrids to differentiate such scenario.
  NGGridPlacementData(const ComputedStyle& grid_style,
                      const NGGridLineResolver& parent_line_resolver,
                      GridArea subgrid_area,
                      wtf_size_t column_auto_repetitions,
                      wtf_size_t row_auto_repetitions)
      : line_resolver(grid_style,
                      parent_line_resolver,
                      subgrid_area,
                      column_auto_repetitions,
                      row_auto_repetitions) {}

  // This constructor only copies inputs to the auto-placement algorithm.
  NGGridPlacementData(const NGGridPlacementData& other)
      : line_resolver(other.line_resolver) {}

  // This method compares the fields computed by the auto-placement algorithm in
  // |NGGridPlacement| and it's only intended to validate the cached data.
  bool operator==(const NGGridPlacementData& other) const {
    return grid_item_positions == other.grid_item_positions &&
           column_start_offset == other.column_start_offset &&
           row_start_offset == other.row_start_offset &&
           line_resolver == other.line_resolver;
  }

  bool operator!=(const NGGridPlacementData& other) const {
    return !(*this == other);
  }

  // TODO(kschmi): Remove placement data from `NGGridPlacement` as well as
  // these helpers.
  bool HasStandaloneAxis(GridTrackSizingDirection track_direction) const {
    return line_resolver.HasStandaloneAxis(track_direction);
  }

  wtf_size_t AutoRepetitions(GridTrackSizingDirection track_direction) const {
    return line_resolver.AutoRepetitions(track_direction);
  }

  wtf_size_t AutoRepeatTrackCount(
      GridTrackSizingDirection track_direction) const {
    return line_resolver.AutoRepeatTrackCount(track_direction);
  }

  wtf_size_t SubgridSpanSize(GridTrackSizingDirection track_direction) const {
    return line_resolver.SubgridSpanSize(track_direction);
  }

  wtf_size_t ExplicitGridTrackCount(
      GridTrackSizingDirection track_direction) const {
    return line_resolver.ExplicitGridTrackCount(track_direction);
  }

  wtf_size_t StartOffset(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_start_offset
                                            : row_start_offset;
  }

  NGGridLineResolver line_resolver;

  // These fields are computed in |NGGridPlacement::RunAutoPlacementAlgorithm|,
  // so they're not considered inputs to the grid placement step.
  Vector<GridArea> grid_item_positions;
  wtf_size_t column_start_offset{0};
  wtf_size_t row_start_offset{0};
};

namespace {

bool AreEqual(const std::unique_ptr<NGGridLayoutTrackCollection>& lhs,
              const std::unique_ptr<NGGridLayoutTrackCollection>& rhs) {
  return (lhs && rhs) ? *lhs == *rhs : !lhs && !rhs;
}

}  // namespace

// This struct contains the column and row data necessary to layout grid items.
// For grid sizing, it will store |NGGridSizingTrackCollection| pointers, which
// are able to modify the geometry of its sets. However, after sizing is done,
// it should only copy |NGGridLayoutTrackCollection| immutable data.
class CORE_EXPORT NGGridLayoutData {
  USING_FAST_MALLOC(NGGridLayoutData);

 public:
  NGGridLayoutData() = default;
  NGGridLayoutData(NGGridLayoutData&&) = default;
  NGGridLayoutData& operator=(NGGridLayoutData&&) = default;

  NGGridLayoutData(const NGGridLayoutData& other) {
    if (other.columns_) {
      columns_ = std::make_unique<NGGridLayoutTrackCollection>(other.Columns());
    }
    if (other.rows_) {
      rows_ = std::make_unique<NGGridLayoutTrackCollection>(other.Rows());
    }
  }

  NGGridLayoutData& operator=(const NGGridLayoutData& other) {
    return *this = NGGridLayoutData(other);
  }

  bool operator==(const NGGridLayoutData& other) const {
    return AreEqual(columns_, other.columns_) && AreEqual(rows_, other.rows_);
  }

  bool operator!=(const NGGridLayoutData& other) const {
    return !(*this == other);
  }

  bool HasSubgriddedAxis(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns)
               ? !(columns_ && columns_->IsForSizing())
               : !(rows_ && rows_->IsForSizing());
  }

  NGGridLayoutTrackCollection& Columns() const {
    DCHECK(columns_ && columns_->Direction() == kForColumns);
    return *columns_;
  }

  NGGridLayoutTrackCollection& Rows() const {
    DCHECK(rows_ && rows_->Direction() == kForRows);
    return *rows_;
  }

  NGGridSizingTrackCollection& SizingCollection(
      GridTrackSizingDirection track_direction) const {
    DCHECK(!HasSubgriddedAxis(track_direction));

    return To<NGGridSizingTrackCollection>(
        (track_direction == kForColumns) ? Columns() : Rows());
  }

  void SetTrackCollection(
      std::unique_ptr<NGGridLayoutTrackCollection> track_collection) {
    DCHECK(track_collection);

    if (track_collection->Direction() == kForColumns) {
      columns_ = std::move(track_collection);
    } else {
      rows_ = std::move(track_collection);
    }
  }

 private:
  std::unique_ptr<NGGridLayoutTrackCollection> columns_;
  std::unique_ptr<NGGridLayoutTrackCollection> rows_;
};

// Subgrid layout relies on the root grid to perform the track sizing algorithm
// for every level of nested subgrids. This class contains the finalized layout
// data of every node in a grid tree (see `ng_grid_subtree.h`), which will be
// passed down to the constraint space of a subgrid to perform layout.
//
// Note that this class allows subtrees to be compared for equality; this is
// important because when we store this tree within a constraint space we want
// to be able to invalidate the cached layout result of a subgrid based on
// whether the provided subtree's track were sized exactly the same.
class NGGridLayoutTree : public RefCounted<NGGridLayoutTree> {
 public:
  struct GridTreeNode {
    NGGridLayoutData layout_data;
    wtf_size_t subtree_size;
  };

  explicit NGGridLayoutTree(wtf_size_t initial_capacity) {
    tree_data_.ReserveInitialCapacity(initial_capacity);
  }

  void Append(const NGGridLayoutData& layout_data, wtf_size_t subtree_size) {
    GridTreeNode grid_node_data{layout_data, subtree_size};
    tree_data_.emplace_back(std::move(grid_node_data));
  }

  bool AreSubtreesEqual(wtf_size_t subtree_root,
                        const NGGridLayoutTree& other,
                        wtf_size_t other_subtree_root) const {
    const wtf_size_t subtree_size = SubtreeSize(subtree_root);

    if (subtree_size != other.SubtreeSize(other_subtree_root)) {
      return false;
    }

    for (wtf_size_t i = 0; i < subtree_size; ++i) {
      if (other.LayoutData(other_subtree_root + i) !=
          LayoutData(subtree_root + i)) {
        return false;
      }
    }
    return true;
  }

  const NGGridLayoutData& LayoutData(wtf_size_t index) const {
    DCHECK_LT(index, tree_data_.size());
    return tree_data_[index].layout_data;
  }

  wtf_size_t Size() const { return tree_data_.size(); }

  wtf_size_t SubtreeSize(wtf_size_t index) const {
    DCHECK_LT(index, tree_data_.size());
    return tree_data_[index].subtree_size;
  }

 private:
  Vector<GridTreeNode, 16> tree_data_;
};

// This class represents a subtree in a `NGGridLayoutTree` and mostly serves two
// purposes: provide seamless iteration over the tree structure and compare
// input subtrees to invalidate a subgrid's cached layout result.
class NGGridLayoutSubtree
    : public NGGridSubtree<NGGridLayoutSubtree,
                           scoped_refptr<const NGGridLayoutTree>> {
  DISALLOW_NEW();

 public:
  NGGridLayoutSubtree() = default;

  explicit NGGridLayoutSubtree(
      scoped_refptr<const NGGridLayoutTree>&& layout_tree)
      : NGGridSubtree(std::move(layout_tree)) {}

  NGGridLayoutSubtree(const scoped_refptr<const NGGridLayoutTree>& layout_tree,
                      wtf_size_t parent_end_index,
                      wtf_size_t subtree_root)
      : NGGridSubtree(layout_tree, parent_end_index, subtree_root) {}

  // This method is meant to be used for layout invalidation, so we only care
  // about comparing the layout data of both subtrees.
  bool operator==(const NGGridLayoutSubtree& other) const {
    return (grid_tree_ && other.grid_tree_)
               ? grid_tree_->AreSubtreesEqual(subtree_root_, *other.grid_tree_,
                                              other.subtree_root_)
               : !grid_tree_ && !other.grid_tree_;
  }

  const NGGridLayoutData& LayoutData() const {
    DCHECK(grid_tree_);
    return grid_tree_->LayoutData(subtree_root_);
  }
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGGridLayoutTree::GridTreeNode)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
