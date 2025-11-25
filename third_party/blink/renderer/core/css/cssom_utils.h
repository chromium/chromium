// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_UTILS_H_

#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSValue;
class CSSValueList;

namespace cssvalue {
class CSSGridTemplateAreasValue;
}

class CSSOMUtils {
  STATIC_ONLY(CSSOMUtils);

 public:
  static bool IncludeDependentGridLineEndValue(const CSSValue* line_start,
                                               const CSSValue* line_end);

  static bool IsAutoValue(const CSSValue* value);

  static bool IsNoneValue(const CSSValue* value);

  static bool IsEmptyValueList(const CSSValue* value);

  static bool HasGridRepeatValue(const CSSValueList* value_list);

  static bool IsGridLanesColumnDirectionValue(
      const CSSValue* grid_lanes_direction_values);

  // Returns the name of a grid area based on the position (`row`, `column`).
  // e.g. with the following grid definition:
  // grid-template-areas: "a a a"
  //                      "b b b";
  // grid-template-rows: [header-top] auto [header-bottom main-top] 1fr
  // [main-bottom]; grid-template-columns: auto 1fr auto;
  //
  // NamedGridAreaTextForPosition(grid_area_map, 0, 0) will return "a"
  // NamedGridAreaTextForPosition(grid_area_map, 1, 0) will return "b"
  //
  // Unlike the CSS indices, these are 0-based indices.
  // Out-of-range or not-found indices return ".", per spec.
  static String NamedGridAreaTextForPosition(
      const NamedGridAreaMap& grid_area_map,
      wtf_size_t row,
      wtf_size_t column);
  // Helper to serialize a single row or column of grid area names into a
  // space-separated string. If `is_row` is true, serialize a row (iterate
  // columns for a fixed row). If `is_row` is false, serialize a column (iterate
  // rows for a fixed column).
  static String SerializeGridAreaText(
      const cssvalue::CSSGridTemplateAreasValue* template_areas,
      wtf_size_t fixed_index,
      bool is_row);
  // Returns a `CSSValueList` containing the computed value for the
  // `grid-template` shorthand, based on provided `grid-template-rows`,
  // `grid-template-columns`, and `grid-template-areas`.
  static CSSValueList* ComputedValueForGridTemplateShorthand(
      const CSSValue* template_row_values,
      const CSSValue* template_column_values,
      const CSSValue* template_area_values);
  // Returns a `CSSValueList` containing the computed value for
  // the `grid-lanes` shorthand, based on provided `grid-template-tracks`,
  // `grid-template-areas`, `grid-lanes-direction`, and `grid-lanes-fill`.
  static CSSValueList* ComputedValueForGridLanesShorthand(
      const CSSValue* grid_template_tracks_values,
      const CSSValue* template_area_values,
      const CSSValue* grid_lanes_direction_values,
      const CSSValue* grid_lanes_fill_values);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_UTILS_H_
