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
  NGGridPlacementData() = default;

  NGGridPlacementData(bool is_subgridded_to_parent,
                      const ComputedStyle& grid_style)
      : line_resolver(grid_style),
        is_subgridded_to_parent(is_subgridded_to_parent) {}

  // Subgrids need to map named lines from every parent grid. This constructor
  // should be used exclusively by subgrids to differentiate such scenario.
  NGGridPlacementData(bool is_subgridded_to_parent,
                      const ComputedStyle& grid_style,
                      const NGGridLineResolver& parent_line_resolver)
      : line_resolver(grid_style, parent_line_resolver),
        is_subgridded_to_parent(is_subgridded_to_parent) {}

  // Do not check `line_resolver` for comparison, as it's used to generate
  // `grid_item_positions` and isn't shared between equivalent instances.
  bool operator==(const NGGridPlacementData& other) const {
    return grid_item_positions == other.grid_item_positions &&
           is_subgridded_to_parent == other.is_subgridded_to_parent &&
           column_subgrid_span_size == other.column_subgrid_span_size &&
           row_subgrid_span_size == other.row_subgrid_span_size &&
           column_auto_repetitions == other.column_auto_repetitions &&
           row_auto_repetitions == other.row_auto_repetitions &&
           column_start_offset == other.column_start_offset &&
           row_start_offset == other.row_start_offset &&
           column_explicit_count == other.column_explicit_count &&
           row_explicit_count == other.row_explicit_count;
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

  NGGridLineResolver line_resolver;
  Vector<GridArea> grid_item_positions;

  bool is_subgridded_to_parent : 1;

  wtf_size_t column_subgrid_span_size{kNotFound};
  wtf_size_t row_subgrid_span_size{kNotFound};
  wtf_size_t column_auto_repetitions{0};
  wtf_size_t row_auto_repetitions{0};
  wtf_size_t column_start_offset{0};
  wtf_size_t row_start_offset{0};
  wtf_size_t column_explicit_count{0};
  wtf_size_t row_explicit_count{0};
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
