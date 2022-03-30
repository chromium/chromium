// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_track_collection.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct CORE_EXPORT NGGridPlacementData {
  NGGridPlacementData() = default;

  explicit NGGridPlacementData(bool is_parent_grid_container)
      : is_parent_grid_container(is_parent_grid_container) {}

  bool operator==(const NGGridPlacementData& other) const {
    return grid_item_positions == other.grid_item_positions &&
           is_parent_grid_container == other.is_parent_grid_container &&
           column_subgrid_span_size == other.column_subgrid_span_size &&
           row_subgrid_span_size == other.row_subgrid_span_size &&
           column_auto_repetitions == other.column_auto_repetitions &&
           row_auto_repetitions == other.row_auto_repetitions &&
           column_start_offset == other.column_start_offset &&
           row_start_offset == other.row_start_offset;
  }

  bool HasStandalonePlacement(GridTrackSizingDirection track_direction) const {
    const wtf_size_t subgrid_span_size = (track_direction == kForColumns)
                                             ? column_subgrid_span_size
                                             : row_subgrid_span_size;
    return subgrid_span_size == kNotFound;
  }

  void SetSubgridSpanSize(wtf_size_t subgrid_span_size,
                          GridTrackSizingDirection track_direction) {
    if (track_direction == kForColumns)
      column_subgrid_span_size = subgrid_span_size;
    else
      row_subgrid_span_size = subgrid_span_size;
  }

  Vector<GridArea> grid_item_positions;

  bool is_parent_grid_container : 1;

  wtf_size_t column_subgrid_span_size{kNotFound};
  wtf_size_t row_subgrid_span_size{kNotFound};
  wtf_size_t column_auto_repetitions{0};
  wtf_size_t row_auto_repetitions{0};
  wtf_size_t column_start_offset{0};
  wtf_size_t row_start_offset{0};
};

// This struct contains the column and row data necessary to layout grid items.
// For grid sizing, it will store |NGGridSizingTrackCollection| pointers, which
// are able to modify the geometry of its sets. However, after sizing is done,
// it should only copy |NGGridLayoutTrackCollection| immutable data.
struct CORE_EXPORT NGGridLayoutData {
  USING_FAST_MALLOC(NGGridLayoutData);

 public:
  NGGridLayoutData() = default;

  NGGridLayoutData(std::unique_ptr<NGGridLayoutTrackCollection> columns,
                   std::unique_ptr<NGGridLayoutTrackCollection> rows)
      : columns(std::move(columns)), rows(std::move(rows)) {}

  NGGridLayoutData(const NGGridLayoutData& other) { CopyFrom(other); }

  NGGridLayoutData& operator=(const NGGridLayoutData& other) {
    CopyFrom(other);
    return *this;
  }

  void CopyFrom(const NGGridLayoutData& other) {
    if (other.columns)
      columns = std::make_unique<NGGridLayoutTrackCollection>(*other.Columns());
    if (other.rows)
      rows = std::make_unique<NGGridLayoutTrackCollection>(*other.Rows());
  }

  NGGridLayoutTrackCollection* Columns() const {
    DCHECK(columns && columns->Direction() == kForColumns);
    return columns.get();
  }

  NGGridLayoutTrackCollection* Rows() const {
    DCHECK(rows && rows->Direction() == kForRows);
    return rows.get();
  }

  std::unique_ptr<NGGridLayoutTrackCollection> columns;
  std::unique_ptr<NGGridLayoutTrackCollection> rows;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_DATA_H_
