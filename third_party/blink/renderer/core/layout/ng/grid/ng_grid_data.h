// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_line_resolver.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
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
                      GridArea subgrid_area)
      : line_resolver(grid_style, parent_line_resolver, subgrid_area) {}

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

  bool HasSubgriddedAxis(GridTrackSizingDirection track_direction) const {
    return !((track_direction == kForColumns)
                 ? columns_ && columns_->IsForSizing()
                 : rows_ && rows_->IsForSizing());
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

  // TODO(ethavar): This two should disappear in the upcoming patch.
  const NGGridLayoutTrackCollection* columns() const { return columns_.get(); }
  const NGGridLayoutTrackCollection* rows() const { return rows_.get(); }

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
