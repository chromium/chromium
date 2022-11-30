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
  NGGridPlacementData(NGGridPlacementData&&) = default;
  NGGridPlacementData& operator=(NGGridPlacementData&&) = default;

  explicit NGGridPlacementData(const ComputedStyle& grid_style)
      : line_resolver(grid_style) {}

  // Subgrids need to map named lines from every parent grid. This constructor
  // should be used exclusively by subgrids to differentiate such scenario.
  NGGridPlacementData(const ComputedStyle& grid_style,
                      const NGGridLineResolver& parent_line_resolver)
      : line_resolver(grid_style, parent_line_resolver) {}

  // This constructor only copies inputs to the auto-placement algorithm.
  NGGridPlacementData(const NGGridPlacementData& other)
      : line_resolver(other.line_resolver),
        subgridded_column_span_size(other.subgridded_column_span_size),
        subgridded_row_span_size(other.subgridded_row_span_size),
        column_auto_repetitions(other.column_auto_repetitions),
        row_auto_repetitions(other.row_auto_repetitions) {}

  // This method compares the fields computed by the auto-placement algorithm in
  // |NGGridPlacement| and it's only intended to validate the cached data.
  bool operator==(const NGGridPlacementData& other) const {
    return grid_item_positions == other.grid_item_positions &&
           explicit_grid_column_count == other.explicit_grid_column_count &&
           explicit_grid_row_count == other.explicit_grid_row_count &&
           column_start_offset == other.column_start_offset &&
           row_start_offset == other.row_start_offset;
  }

  bool HasStandaloneAxis(GridTrackSizingDirection track_direction) const {
    const wtf_size_t subgrid_span_size = (track_direction == kForColumns)
                                             ? subgridded_column_span_size
                                             : subgridded_row_span_size;
    return subgrid_span_size == kNotFound;
  }

  bool IsSubgriddedToParent() const {
    return subgridded_column_span_size != kNotFound ||
           subgridded_row_span_size != kNotFound;
  }

  NGGridLineResolver line_resolver;
  wtf_size_t subgridded_column_span_size{kNotFound};
  wtf_size_t subgridded_row_span_size{kNotFound};
  wtf_size_t column_auto_repetitions{1};
  wtf_size_t row_auto_repetitions{1};

  // These fields are computed in |NGGridPlacement::RunAutoPlacementAlgorithm|,
  // so they're not considered inputs to the grid placement step.
  Vector<GridArea> grid_item_positions;
  wtf_size_t explicit_grid_column_count{0};
  wtf_size_t explicit_grid_row_count{0};
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
