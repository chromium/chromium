// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_line_resolver.h"
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
// for every level of nested subgrids. This class is a collection of finalized
// layout data of every grid/subgrid in the entire grid tree, which will be
// passed down to the constraint space of a subgrid to perform layout.
//
// The tree is represented by two vectors satisfying the following conditions:
//   - The nodes in the tree are indexed using preorder traversal.
//   - Each subtree is guaranteed to be contained in a single contiguous range.
//   - We can iterate over a node's children by skipping over their subtrees;
//   i.e., the first child of a node `k` is always at position `k+1`, the next
//   sibling comes `subtree_size_[k+1]` positions later, and so on.
//
//         (0)
//        /   \
//     (1)     (7)
//     / \     / \
//   (2) (5) (8) (9)
//   / \   \
// (3) (4) (6)        (0)
//                       (1)               (7)
//                          (2)      (5)      (8)(9)
//                             (3)(4)   (6)
//   subtree_size_ = [10, 6, 3, 1, 1, 2, 1, 3, 1, 1]
//
// Note that this class allows subtrees to be compared for equality; this is
// important because when we store this tree within a constraint space we want
// to be able to invalidate the cached layout result of a subgrid based on
// whether the provided subtree's track were sized exactly the same.
class NGGridLayoutTree : public RefCounted<NGGridLayoutTree> {
 public:
  explicit NGGridLayoutTree(wtf_size_t size = 1) {
    subtree_size_.ReserveInitialCapacity(size);
    layout_data_.ReserveInitialCapacity(size);
  }

  bool AreSubtreesEqual(wtf_size_t subtree_root,
                        const NGGridLayoutTree& other,
                        wtf_size_t other_subtree_root) const {
    DCHECK_LT(other_subtree_root, other.subtree_size_.size());
    DCHECK_LT(subtree_root, subtree_size_.size());

    const wtf_size_t subtree_size = subtree_size_[subtree_root];
    if (subtree_size != other.subtree_size_[other_subtree_root]) {
      return false;
    }

    DCHECK_LE(other_subtree_root + subtree_size, other.layout_data_.size());
    DCHECK_LE(subtree_root + subtree_size, layout_data_.size());

    for (wtf_size_t i = 0; i < subtree_size; ++i) {
      if (other.layout_data_[other_subtree_root + i] !=
          layout_data_[subtree_root + i]) {
        return false;
      }
    }
    return true;
  }

  const NGGridLayoutData& LayoutData(wtf_size_t index) const {
    DCHECK_LT(index, layout_data_.size());
    return layout_data_[index];
  }

  wtf_size_t Size() const {
    DCHECK_EQ(layout_data_.size(), subtree_size_.size());
    return layout_data_.size();
  }

  wtf_size_t SubtreeSize(wtf_size_t index) const {
    DCHECK_LT(index, subtree_size_.size());
    return subtree_size_[index];
  }

  void Append(const NGGridLayoutData& layout_data, wtf_size_t subtree_size) {
    subtree_size_.emplace_back(subtree_size);
    layout_data_.emplace_back(layout_data);
  }

 private:
  // Holds the finalized layout data of each grid in the tree.
  Vector<NGGridLayoutData, 16> layout_data_;

  // The size of the subtree rooted at each grid node. For a given index `k`,
  // this means that the range [k, k + subtree_size_[k]) in both vectors
  // represent the data of the subtree rooted at grid node `k`.
  Vector<wtf_size_t, 16> subtree_size_;
};

// This class represents a subtree in a `NGGridLayoutTree` and mostly serves two
// purposes: provide seamless iteration over the tree structure and compare
// input subtrees to invalidate a subgrid's cached layout result.
//
// A subtree is represented by a pointer to the original layout tree's data (see
// `NGGridLayoutTree` description) and the index of the subtree's root. However,
// in order to iterate over the siblings of a given subtree we need to store the
// index of the next sibling of its parent, aka the parent's end index, so that
// the iterator doesn't traverse outside of the parent's subtree, e.g.:
//
//                                 (0)
//                                    (1)               (7)
//                                       (2)      (5)      (8)(9)
//                                          (3)(4)   (6)
//   layout_tree_.subtree_size_ = [10, 6, 3, 1, 1, 2, 1, 3, 1, 1]
//
// We can compute the next sibling of a subtree rooted at index 2 by adding the
// subtree size at that index (2 + 3 = 5). On the other hand, when we want to
// compute the next sibling for the subtree at index 5, adding the subtree size
// (5 + 2) it's equal to its parent's next sibling (aka parent's end index), so
// we can determine that such subtree doesn't have a next sibling.

class NGGridLayoutSubtree {
  DISALLOW_NEW();

 public:
  NGGridLayoutSubtree() = default;

  explicit NGGridLayoutSubtree(
      scoped_refptr<const NGGridLayoutTree>&& layout_tree)
      : layout_tree_(std::move(layout_tree)), subtree_root_(0) {
    DCHECK(layout_tree_);
    parent_end_index_ = layout_tree_->Size();
  }

  NGGridLayoutSubtree(const scoped_refptr<const NGGridLayoutTree>& layout_tree,
                      wtf_size_t parent_end_index,
                      wtf_size_t subtree_root) {
    DCHECK(layout_tree);
    DCHECK_LE(parent_end_index, layout_tree->Size());
    DCHECK_LE(subtree_root, parent_end_index);

    // If the subtree root is beyond the parent's end index, we will keep this
    // instance as a null subtree to indicate the end iterator for siblings.
    if (subtree_root < parent_end_index) {
      layout_tree_ = layout_tree;
      parent_end_index_ = parent_end_index;
      subtree_root_ = subtree_root;
    }
  }

  explicit operator bool() const { return static_cast<bool>(layout_tree_); }

  // This method is meant to be used for layout invalidation, so we only care
  // about comparing the layout data of both subtrees.
  bool operator==(const NGGridLayoutSubtree& other) const {
    return (layout_tree_ && other.layout_tree_)
               ? layout_tree_->AreSubtreesEqual(
                     subtree_root_, *other.layout_tree_, other.subtree_root_)
               : !layout_tree_ && !other.layout_tree_;
  }

  // First child is always at `subtree_root_ + 1` (see `NGGridLayoutTree`).
  NGGridLayoutSubtree FirstChild() const {
    return NGGridLayoutSubtree(layout_tree_,
                               /* parent_end_index */ NextSiblingIndex(),
                               /* subtree_root */ subtree_root_ + 1);
  }

  NGGridLayoutSubtree NextSibling() const {
    return NGGridLayoutSubtree(layout_tree_,
                               /* parent_end_index */ parent_end_index_,
                               /* subtree_root */ NextSiblingIndex());
  }

  const NGGridLayoutData& LayoutData() const {
    DCHECK(layout_tree_);
    return layout_tree_->LayoutData(subtree_root_);
  }

 private:
  wtf_size_t NextSiblingIndex() const {
    DCHECK(layout_tree_);
    return subtree_root_ + layout_tree_->SubtreeSize(subtree_root_);
  }

  // Pointer to the layout tree data shared by multiple subtree instances.
  scoped_refptr<const NGGridLayoutTree> layout_tree_;

  // Index of the next sibling of this subtree's parent; used to avoid iterating
  // outside of the parent's subtree when computing this subtree's next sibling.
  wtf_size_t parent_end_index_{kNotFound};

  // Index of this subtree's root node.
  wtf_size_t subtree_root_{kNotFound};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
