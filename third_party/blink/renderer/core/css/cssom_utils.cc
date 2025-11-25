// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom_utils.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"

namespace blink {

// static
bool CSSOMUtils::IncludeDependentGridLineEndValue(const CSSValue* line_start,
                                                  const CSSValue* line_end) {
  const bool line_end_is_initial_value =
      IsA<CSSIdentifierValue>(line_end) &&
      To<CSSIdentifierValue>(line_end)->GetValueID() == CSSValueID::kAuto;

  // "When grid-column-start is omitted, if grid-row-start is a <custom-ident>,
  // all four longhands are set to that value. Otherwise, it is set to auto.
  // When grid-row-end is omitted, if grid-row-start is a <custom-ident>,
  // grid-row-end is set to that <custom-ident>; otherwise, it is set to auto.
  // When grid-column-end is omitted, if grid-column-start is a <custom-ident>,
  // grid-column-end is set to that <custom-ident>; otherwise, it is set to
  // auto."
  //
  // https://www.w3.org/TR/css-grid-2/#placement-shorthands
  //
  // In order to produce a shortest-possible-serialization, we need essentially
  // the converse of that statement, as parsing handles the
  // literal interpretation. In particular, `CSSValueList` values (integer
  // literals) are always included, duplicate `custom-ident` values get
  // dropped, as well as initial values if they match the equivalent
  // `line_start` value.
  return IsA<CSSValueList>(line_end) ||
         ((*line_end != *line_start) &&
          (IsA<CSSCustomIdentValue>(line_start) || !line_end_is_initial_value));
}

// static
bool CSSOMUtils::IsAutoValue(const CSSValue* value) {
  return IsA<CSSIdentifierValue>(value) &&
         To<CSSIdentifierValue>(value)->GetValueID() == CSSValueID::kAuto;
}

// static
bool CSSOMUtils::IsNoneValue(const CSSValue* value) {
  return IsA<CSSIdentifierValue>(value) &&
         To<CSSIdentifierValue>(value)->GetValueID() == CSSValueID::kNone;
}

// static
bool CSSOMUtils::IsEmptyValueList(const CSSValue* value) {
  const CSSValueList* value_list = DynamicTo<CSSValueList>(value);
  return value_list && value_list->length() == 0;
}

// static
bool CSSOMUtils::HasGridRepeatValue(const CSSValueList* value_list) {
  if (value_list) {
    for (const auto& value : *value_list) {
      if (value->IsGridRepeatValue()) {
        return true;
      }
    }
  }
  return false;
}

// static
bool CSSOMUtils::IsGridLanesColumnDirectionValue(
    const CSSValue* grid_lanes_direction_values) {
  const auto* grid_lanes_direction_value =
      DynamicTo<CSSIdentifierValue>(grid_lanes_direction_values);
  return grid_lanes_direction_value &&
         (grid_lanes_direction_value->GetValueID() == CSSValueID::kColumn ||
          grid_lanes_direction_value->GetValueID() ==
              CSSValueID::kColumnReverse);
}

// static
String CSSOMUtils::NamedGridAreaTextForPosition(
    const NamedGridAreaMap& grid_area_map,
    wtf_size_t row,
    wtf_size_t column) {
  for (const auto& item : grid_area_map) {
    const GridArea& area = item.value;
    if (row >= area.rows.StartLine() && row < area.rows.EndLine() &&
        column >= area.columns.StartLine() && column < area.columns.EndLine()) {
      return item.key;
    }
  }
  return ".";
}

// static
String CSSOMUtils::SerializeGridAreaText(
    const cssvalue::CSSGridTemplateAreasValue* template_areas,
    wtf_size_t fixed_index,
    bool is_row) {
  const NamedGridAreaMap& grid_area_map = template_areas->GridAreaMap();
  const wtf_size_t count =
      is_row ? template_areas->ColumnCount() : template_areas->RowCount();
  StringBuilder result;
  for (wtf_size_t i = 0; i < count; ++i) {
    if (is_row) {
      result.Append(
          NamedGridAreaTextForPosition(grid_area_map, fixed_index, i));
    } else {
      result.Append(
          NamedGridAreaTextForPosition(grid_area_map, i, fixed_index));
    }
    if (i != count - 1) {
      result.Append(' ');
    }
  }
  return result.ReleaseString();
}

// static
CSSValueList* CSSOMUtils::ComputedValueForGridTemplateShorthand(
    const CSSValue* template_row_values,
    const CSSValue* template_column_values,
    const CSSValue* template_area_values) {
  const bool has_initial_template_rows = IsNoneValue(template_row_values);
  const bool has_initial_template_columns = IsNoneValue(template_column_values);
  const bool has_initial_template_areas =
      !template_area_values || IsNoneValue(template_area_values);

  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  // 1- 'none' case.
  if (has_initial_template_areas && has_initial_template_rows &&
      has_initial_template_columns) {
    list->Append(*template_row_values);
    return list;
  }

  // It is invalid to specify `grid-template-areas` without
  // `grid-template-rows`.
  if (!has_initial_template_areas && has_initial_template_rows) {
    return list;
  }

  // 2- <grid-template-rows> / <grid-template-columns>
  if (!IsA<CSSValueList>(template_row_values) || has_initial_template_areas) {
    list->Append(*template_row_values);
    list->Append(*template_column_values);

    return list;
  }

  // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+
  // [ / <track-list> ]?
  //
  // "Note that the repeat() function isn’t allowed in these track listings, as
  // the tracks are intended to visually line up one-to-one with the
  // rows/columns in the “ASCII art”."
  //
  // https://www.w3.org/TR/css-grid-2/#explicit-grid-shorthand
  const CSSValueList* template_row_value_list =
      DynamicTo<CSSValueList>(template_row_values);
  DCHECK(template_row_value_list);
  if (HasGridRepeatValue(template_row_value_list) ||
      HasGridRepeatValue(DynamicTo<CSSValueList>(template_column_values))) {
    return list;
  }

  // In this serialization, there must be a value for grid-areas.
  const cssvalue::CSSGridTemplateAreasValue* template_areas =
      DynamicTo<cssvalue::CSSGridTemplateAreasValue>(template_area_values);
  DCHECK(template_areas);

  // Handle [ <line-names>? <string> <track-size>? <line-names>? ]+
  CSSValueList* template_row_list = CSSValueList::CreateSpaceSeparated();
  wtf_size_t row = 0;
  for (const auto& row_value : *template_row_value_list) {
    if (row_value->IsGridLineNamesValue()) {
      template_row_list->Append(*row_value);
      continue;
    }
    String grid_area_text =
        SerializeGridAreaText(template_areas, row, /*is_row=*/true);
    DCHECK(!grid_area_text.empty());
    template_row_list->Append(
        *MakeGarbageCollected<CSSStringValue>(grid_area_text));
    ++row;

    // Omit `auto` values.
    if (!IsAutoValue(row_value.Get())) {
      template_row_list->Append(*row_value);
    }
  }

  // If the actual number of rows serialized via `grid-template-rows` doesn't
  // match the rows defined via grid-areas, the shorthand cannot be serialized
  // and we must return the empty string.
  if (row != template_areas->RowCount()) {
    return list;
  }

  list->Append(*template_row_list);

  // Handle [ / <track-list> ]?
  if (!has_initial_template_columns) {
    list->Append(*template_column_values);
  }

  return list;
}

// static
CSSValueList* CSSOMUtils::ComputedValueForGridLanesShorthand(
    const CSSValue* grid_template_tracks_values,
    const CSSValue* template_area_values,
    const CSSValue* grid_lanes_direction_values,
    const CSSValue* grid_lanes_fill_values) {
  const bool has_initial_grid_template_tracks =
      IsNoneValue(grid_template_tracks_values);
  const bool has_initial_template_areas = IsNoneValue(template_area_values);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (has_initial_template_areas && has_initial_grid_template_tracks) {
    list->Append(*template_area_values);
  }

  if (!has_initial_template_areas) {
    // If we have template columns, we can serialize the template areas as is.
    // Otherwise, for template rows, we need to serialize multiple string tokens
    // into a single space-separated string.
    if (IsGridLanesColumnDirectionValue(grid_lanes_direction_values)) {
      list->Append(*template_area_values);
    } else {
      const cssvalue::CSSGridTemplateAreasValue* template_areas =
          DynamicTo<cssvalue::CSSGridTemplateAreasValue>(template_area_values);
      DCHECK(template_areas);
      String template_area_text = SerializeGridAreaText(
          template_areas, /*fixed_index=*/0, /*is_row=*/false);
      list->Append(*MakeGarbageCollected<CSSStringValue>(template_area_text));
    }
  }

  if (!has_initial_grid_template_tracks) {
    list->Append(*grid_template_tracks_values);
  }

  list->Append(*grid_lanes_direction_values);
  list->Append(*grid_lanes_fill_values);

  return list;
}

}  // namespace blink
