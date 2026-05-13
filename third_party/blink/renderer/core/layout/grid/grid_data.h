// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_DATA_H_

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/grid/grid_line_resolver.h"
#include "third_party/blink/renderer/core/layout/grid/grid_subtree.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct CORE_EXPORT GridPlacementData {
  DISALLOW_NEW();

 public:
  GridPlacementData(GridPlacementData&&) = default;
  GridPlacementData& operator=(GridPlacementData&&) = default;

  explicit GridPlacementData(const GridLineResolver& line_resolver)
      : line_resolver(line_resolver) {}

  bool operator==(const GridPlacementData& other) const {
    return grid_item_positions == other.grid_item_positions &&
           column_start_offset == other.column_start_offset &&
           row_start_offset == other.row_start_offset &&
           line_resolver == other.line_resolver;
  }

  wtf_size_t AutoRepeatTrackCount(
      GridTrackSizingDirection track_direction) const {
    return line_resolver.AutoRepeatTrackCount(track_direction);
  }

  wtf_size_t ExplicitGridTrackCount(
      GridTrackSizingDirection track_direction) const {
    return line_resolver.ExplicitGridTrackCount(track_direction);
  }

  bool HasStandaloneAxis(GridTrackSizingDirection track_direction) const {
    return line_resolver.HasStandaloneAxis(track_direction);
  }

  wtf_size_t StartOffset(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_start_offset
                                            : row_start_offset;
  }

  wtf_size_t SubgridSpanSize(GridTrackSizingDirection track_direction) const {
    return line_resolver.SubgridSpanSize(track_direction);
  }

  GridLineResolver line_resolver;
  Vector<GridArea> grid_item_positions;
  wtf_size_t column_start_offset{0};
  wtf_size_t row_start_offset{0};
};

// This struct contains the column and row data necessary to layout grid items.
// For grid sizing, it will store |GridSizingTrackCollection| pointers, which
// are able to modify the geometry of its sets. However, after sizing is done,
// it should only copy |GridLayoutTrackCollection| immutable data.
class CORE_EXPORT GridLayoutData : public GarbageCollected<GridLayoutData> {
 public:
  GridLayoutData() = default;
  GridLayoutData(GridLayoutData&&) = delete;
  GridLayoutData& operator=(GridLayoutData&&) = delete;

  GridLayoutData(const GridLayoutData& other) {
    // Track collections are pure geometry; always shallow-copy.
    columns_ = other.columns_;
    rows_ = other.rows_;
    // Deep-copy baselines since they may be mutated per-subgrid.
    if (other.column_baselines_) {
      column_baselines_ =
          MakeGarbageCollected<GridTrackBaselines>(*other.column_baselines_);
    }
    if (other.row_baselines_) {
      row_baselines_ =
          MakeGarbageCollected<GridTrackBaselines>(*other.row_baselines_);
    }
  }

  bool operator==(const GridLayoutData& other) const {
    return base::ValuesEquivalent(columns_, other.columns_) &&
           base::ValuesEquivalent(rows_, other.rows_) &&
           base::ValuesEquivalent(column_baselines_, other.column_baselines_) &&
           base::ValuesEquivalent(row_baselines_, other.row_baselines_);
  }

  bool HasSubgriddedAxis(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns)
               ? !(columns_ && columns_->IsForSizing())
               : !(rows_ && rows_->IsForSizing());
  }

  bool HasTrackCollection(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? !!columns_ : !!rows_;
  }

  bool IsSubgridWithStandaloneAxis(
      GridTrackSizingDirection track_direction) const {
    return columns_ && rows_ &&
           ((track_direction == kForColumns)
                ? columns_->IsForSizing() && !rows_->IsForSizing()
                : rows_->IsForSizing() && !columns_->IsForSizing());
  }

  GridLayoutTrackCollection& Columns() const {
    DCHECK(columns_);
    DCHECK_EQ(columns_->Direction(), kForColumns);
    return *columns_;
  }

  GridLayoutTrackCollection& Rows() const {
    DCHECK(rows_);
    DCHECK_EQ(rows_->Direction(), kForRows);
    return *rows_;
  }

  GridSizingTrackCollection& SizingCollection(
      GridTrackSizingDirection track_direction) const {
    DCHECK(!HasSubgriddedAxis(track_direction));

    return To<GridSizingTrackCollection>(
        (track_direction == kForColumns) ? Columns() : Rows());
  }

  // This method is intended for subgrids with both a standalone and a
  // subgridded axis. Returns the only subgridded track collection.
  const GridLayoutTrackCollection* OnlySubgriddedCollection() const {
    DCHECK(columns_);
    DCHECK(rows_);
    DCHECK_NE(columns_->IsForSizing(), rows_->IsForSizing());
    return columns_->IsForSizing() ? rows_.Get() : columns_.Get();
  }

  void SetTrackCollection(GridLayoutTrackCollection* track_collection) {
    DCHECK(track_collection);

    if (track_collection->Direction() == kForColumns) {
      columns_ = track_collection;
    } else {
      rows_ = track_collection;
    }
  }

  // Returns true if any existing axis has an indefinite set. Handles the case
  // where one axis may not have a track collection (e.g., the stacking axis in
  // grid-lanes).
  bool HasIndefiniteSet() const {
    return (columns_ && Columns().HasIndefiniteSet()) ||
           (rows_ && Rows().HasIndefiniteSet());
  }

  bool HasBaselines(GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? !!column_baselines_
                                            : !!row_baselines_;
  }

  LayoutUnit MajorBaseline(GridTrackSizingDirection track_direction,
                           wtf_size_t set_index) const {
    const auto* baselines = (track_direction == kForColumns)
                                ? column_baselines_.Get()
                                : row_baselines_.Get();
    if (!baselines || set_index >= baselines->major.size()) {
      return LayoutUnit::Min();
    }
    return baselines->major[set_index];
  }

  LayoutUnit MinorBaseline(GridTrackSizingDirection track_direction,
                           wtf_size_t set_index) const {
    const auto* baselines = (track_direction == kForColumns)
                                ? column_baselines_.Get()
                                : row_baselines_.Get();
    if (!baselines || set_index >= baselines->minor.size()) {
      return LayoutUnit::Min();
    }
    return baselines->minor[set_index];
  }

  void CreateBaselines(GridTrackSizingDirection track_direction) {
    auto& baselines =
        (track_direction == kForColumns) ? column_baselines_ : row_baselines_;
    baselines = MakeGarbageCollected<GridTrackBaselines>();
  }

  void ResetBaselines(GridTrackSizingDirection track_direction,
                      wtf_size_t set_count) {
    auto& baselines =
        (track_direction == kForColumns) ? column_baselines_ : row_baselines_;
    if (!baselines) {
      baselines = MakeGarbageCollected<GridTrackBaselines>();
    }
    baselines->Reset(set_count);
  }

  void SetMajorBaseline(GridTrackSizingDirection track_direction,
                        wtf_size_t set_index,
                        LayoutUnit candidate_baseline) {
    auto* baselines = (track_direction == kForColumns) ? column_baselines_.Get()
                                                       : row_baselines_.Get();
    DCHECK(baselines && set_index < baselines->major.size());
    if (candidate_baseline > baselines->major[set_index]) {
      baselines->major[set_index] = candidate_baseline;
    }
  }

  void SetMinorBaseline(GridTrackSizingDirection track_direction,
                        wtf_size_t set_index,
                        LayoutUnit candidate_baseline) {
    auto* baselines = (track_direction == kForColumns) ? column_baselines_.Get()
                                                       : row_baselines_.Get();
    DCHECK(baselines && set_index < baselines->minor.size());
    if (candidate_baseline > baselines->minor[set_index]) {
      baselines->minor[set_index] = candidate_baseline;
    }
  }

  void SetBaselines(GridTrackSizingDirection track_direction,
                    GridTrackBaselines* baselines) {
    if (track_direction == kForColumns) {
      column_baselines_ = baselines;
    } else {
      row_baselines_ = baselines;
    }
  }

  const GridTrackBaselines* GetBaselines(
      GridTrackSizingDirection track_direction) const {
    return (track_direction == kForColumns) ? column_baselines_.Get()
                                            : row_baselines_.Get();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(columns_);
    visitor->Trace(rows_);
    visitor->Trace(column_baselines_);
    visitor->Trace(row_baselines_);
  }

  const HashMap<GridTrackSize, LayoutUnit>* IntrinsicRepeatTrackSizes() const {
    if (intrinsic_repeat_track_sizes_.has_value()) {
      return &intrinsic_repeat_track_sizes_.value();
    }
    return nullptr;
  }

  void AppendIntrinsicRepeatTrackSize(const GridTrackSize& track_size,
                                      LayoutUnit size) {
    if (!intrinsic_repeat_track_sizes_.has_value()) {
      intrinsic_repeat_track_sizes_.emplace();
    }
    auto it = intrinsic_repeat_track_sizes_->find(track_size);
    if (it == intrinsic_repeat_track_sizes_->end()) {
      intrinsic_repeat_track_sizes_->Set(track_size, size);
    } else {
      // If multiple tracks of the same definition within the repeat() resolve
      // to different sizes, take the largest size to use when calculating the
      // final number of auto repetitions.
      it->value = max(it->value, size);
    }
  }

 private:
  Member<GridLayoutTrackCollection> columns_;
  Member<GridLayoutTrackCollection> rows_;

  Member<GridTrackBaselines> column_baselines_;
  Member<GridTrackBaselines> row_baselines_;

  // Intrinsic repeat track sizes for grid-lanes. Used across sizing passes
  // to store the resolved sizes of intrinsic tracks within a repeat()
  // definition.
  std::optional<HashMap<GridTrackSize, LayoutUnit>>
      intrinsic_repeat_track_sizes_;
};

// Subgrid layout relies on the root grid to perform the track sizing algorithm
// for every level of nested subgrids. This class contains the finalized layout
// data of every node in a grid tree (see `grid_subtree.h`), which will be
// passed down to the constraint space of a subgrid to perform layout.
//
// Note that this class allows subtrees to be compared for equality; this is
// important because when we store this tree within a constraint space we want
// to be able to invalidate the cached layout result of a subgrid based on
// whether the provided subtree's track were sized exactly the same.
class GridLayoutTree : public GarbageCollected<GridLayoutTree> {
 public:
  struct GridTreeNode : public GarbageCollected<GridTreeNode> {
    GridTreeNode(GridLayoutData* layout_data, wtf_size_t subtree_size)
        : has_unresolved_geometry(layout_data->HasIndefiniteSet()),
          layout_data(layout_data),
          subtree_size(subtree_size) {}

    void Trace(Visitor* visitor) const { visitor->Trace(layout_data); }

    bool has_unresolved_geometry;
    Member<GridLayoutData> layout_data;
    wtf_size_t subtree_size;
  };

  explicit GridLayoutTree(HeapVector<Member<GridTreeNode>, 16>&& tree_data)
      : tree_data_(std::move(tree_data)) {}

  bool AreSubtreesEqual(wtf_size_t subtree_root,
                        const GridLayoutTree& other,
                        wtf_size_t other_subtree_root) const {
    const wtf_size_t subtree_size = SubtreeSize(subtree_root);

    if (subtree_size != other.SubtreeSize(other_subtree_root)) {
      return false;
    }

    for (wtf_size_t i = 0; i < subtree_size; ++i) {
      if (*other.LayoutData(other_subtree_root + i) !=
          *LayoutData(subtree_root + i)) {
        return false;
      }
    }
    return true;
  }

  bool HasUnresolvedGeometry(wtf_size_t index) const {
    DCHECK_LT(index, tree_data_.size());
    return tree_data_[index]->has_unresolved_geometry;
  }

  GridLayoutData* LayoutData(wtf_size_t index) const {
    DCHECK_LT(index, tree_data_.size());
    return tree_data_[index]->layout_data.Get();
  }

  wtf_size_t Size() const { return tree_data_.size(); }

  wtf_size_t SubtreeSize(wtf_size_t index) const {
    DCHECK_LT(index, tree_data_.size());
    return tree_data_[index]->subtree_size;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(tree_data_); }

 private:
  HeapVector<Member<GridTreeNode>, 16> tree_data_;
};

// This class represents a subtree in a `GridLayoutTree` and mostly serves two
// purposes: provide seamless iteration over the tree structure and compare
// input subtrees to invalidate a subgrid's cached layout result.
class GridLayoutSubtree : public GarbageCollected<GridLayoutSubtree>,
                          public GridSubtree<GridLayoutTree> {
 public:
  GridLayoutSubtree() = default;

  explicit GridLayoutSubtree(const GridLayoutTree* layout_tree,
                             wtf_size_t subtree_root = 0)
      : layout_tree_(layout_tree) {
    SetSubtreeRoot(LayoutTree(), subtree_root);
  }

  GridLayoutSubtree(const GridLayoutTree* layout_tree, GridSubtree subtree)
      : GridSubtree(std::move(subtree)), layout_tree_(layout_tree) {}

  GridLayoutSubtree* FirstChild() const {
    return MakeGarbageCollected<GridLayoutSubtree>(
        layout_tree_, GridSubtree::FirstChild(LayoutTree()));
  }

  GridLayoutSubtree* NextSibling() const {
    return MakeGarbageCollected<GridLayoutSubtree>(
        layout_tree_, GridSubtree::NextSibling(LayoutTree()));
  }

  // This method is meant to be used for layout invalidation, so we only care
  // about comparing the layout data of both subtrees.
  bool operator==(const GridLayoutSubtree& other) const {
    return (layout_tree_ && other.layout_tree_)
               ? layout_tree_->AreSubtreesEqual(
                     subtree_root_, *other.layout_tree_, other.subtree_root_)
               : !layout_tree_ && !other.layout_tree_;
  }

  bool HasUnresolvedGeometry() const {
    return LayoutTree().HasUnresolvedGeometry(subtree_root_);
  }

  GridLayoutData* LayoutData() const {
    return LayoutTree().LayoutData(subtree_root_);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(layout_tree_); }

 private:
  const GridLayoutTree& LayoutTree() const {
    DCHECK(layout_tree_);
    return *layout_tree_;
  }

  // Pointer to the layout tree shared by multiple subtree instances.
  Member<const GridLayoutTree> layout_tree_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GRID_GRID_DATA_H_
