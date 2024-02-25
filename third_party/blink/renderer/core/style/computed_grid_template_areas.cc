// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/computed_grid_template_areas.h"

namespace blink {

// static
NamedGridLinesMap
ComputedGridTemplateAreas::CreateImplicitNamedGridLinesFromGridArea(
    const NamedGridAreaMap& named_areas,
    GridTrackSizingDirection direction) {
  NamedGridLinesMap named_grid_lines;
  for (const auto& named_area : named_areas) {
    GridSpan area_span = direction == kForRows ? named_area.value.rows
                                               : named_area.value.columns;
    {
      NamedGridLinesMap::AddResult start_result = named_grid_lines.insert(
          named_area.key + "-start", Vector<wtf_size_t>());
      start_result.stored_value->value.push_back(area_span.StartLine());
      std::sort(start_result.stored_value->value.begin(),
                start_result.stored_value->value.end());
    }
    {
      NamedGridLinesMap::AddResult end_result = named_grid_lines.insert(
          named_area.key + "-end", Vector<wtf_size_t>());
      end_result.stored_value->value.push_back(area_span.EndLine());
      std::sort(end_result.stored_value->value.begin(),
                end_result.stored_value->value.end());
    }
  }
  return named_grid_lines;
}

ComputedGridTemplateAreas::ComputedGridTemplateAreas(
    const NamedGridAreaMap& named_areas,
    wtf_size_t row_count,
    wtf_size_t column_count)
    : named_areas(named_areas),
      implicit_named_grid_row_lines(
          CreateImplicitNamedGridLinesFromGridArea(named_areas, kForRows)),
      implicit_named_grid_column_lines(
          CreateImplicitNamedGridLinesFromGridArea(named_areas, kForColumns)),
      row_count(row_count),
      column_count(column_count) {}

}  // namespace blink
