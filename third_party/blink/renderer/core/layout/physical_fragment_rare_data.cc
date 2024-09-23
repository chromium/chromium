// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/physical_fragment_rare_data.h"

#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/frame_set_layout_data.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

PhysicalFragmentRareData::PhysicalFragmentRareData(wtf_size_t num_fields) {
  field_list_.ReserveInitialCapacity(num_fields);
}

PhysicalFragmentRareData::PhysicalFragmentRareData(
    const PhysicalRect* scrollable_overflow,
    const PhysicalBoxStrut* borders,
    const PhysicalBoxStrut* scrollbar,
    const PhysicalBoxStrut* padding,
    std::optional<PhysicalRect> inflow_bounds,
    BoxFragmentBuilder& builder,
    wtf_size_t num_fields)
    : table_collapsed_borders_(builder.table_collapsed_borders_),
      mathml_paint_info_(builder.mathml_paint_info_),
      reading_flow_elements_(
          builder.reading_flow_elements_.size()
              ? MakeGarbageCollected<HeapVector<Member<Element>>>(
                    builder.reading_flow_elements_)
              : nullptr) {
  field_list_.ReserveInitialCapacity(num_fields);

  // Each field should be processed in order of FieldId to avoid vector
  // element insertions.

  if (scrollable_overflow) {
    SetField(FieldId::kScrollableOverflow).scrollable_overflow =
        *scrollable_overflow;
  }
  if (borders) {
    SetField(FieldId::kBorders).borders = *borders;
  }
  if (scrollbar) {
    SetField(FieldId::kScrollbar).scrollbar = *scrollbar;
  }
  if (padding) {
    SetField(FieldId::kPadding).padding = *padding;
  }
  if (inflow_bounds) {
    SetField(FieldId::kInflowBounds).inflow_bounds = *inflow_bounds;
  }
  if (builder.frame_set_layout_data_) {
    SetField(FieldId::kFrameSetLayoutData).frame_set_layout_data =
        std::move(builder.frame_set_layout_data_);
  }
  if (builder.table_grid_rect_) {
    SetField(FieldId::kTableGridRect).table_grid_rect =
        *builder.table_grid_rect_;
  }
  if (builder.table_collapsed_borders_geometry_) {
    SetField(FieldId::kTableCollapsedBordersGeometry)
        .table_collapsed_borders_geometry =
        std::move(builder.table_collapsed_borders_geometry_);
  }
  if (builder.table_cell_column_index_) {
    SetField(FieldId::kTableCellColumnIndex).table_cell_column_index =
        *builder.table_cell_column_index_;
  }
  if (!builder.table_section_row_offsets_.empty()) {
    SetField(FieldId::kTableSectionStartRowIndex)
        .table_section_start_row_index = builder.table_section_start_row_index_;
    SetField(FieldId::kTableSectionRowOffsets).table_section_row_offsets =
        std::move(builder.table_section_row_offsets_);
  }
  if (builder.page_name_) {
    SetField(FieldId::kPageName).page_name = builder.page_name_;
  }

  if (!builder.table_column_geometries_.empty()) {
    table_column_geometries_ =
        MakeGarbageCollected<TableFragmentData::ColumnGeometries>(
            builder.table_column_geometries_);
  }

  // size() can be smaller than num_fields because FieldId::kMargins is not
  // set yet.
  DCHECK_LE(field_list_.size(), num_fields);
}

#define SET_IF_EXISTS(id, name, source)                   \
  if (const auto* field = source.GetField(FieldId::id)) { \
    SetField(FieldId::id).name = field->name;             \
  }
#define CLONE_IF_EXISTS(id, name, source)                                    \
  if (const auto* field = source.GetField(FieldId::id)) {                    \
    SetField(FieldId::id).name =                                             \
        std::make_unique<decltype(field->name)::element_type>(*field->name); \
  }

PhysicalFragmentRareData::PhysicalFragmentRareData(
    const PhysicalFragmentRareData& other)
    : table_collapsed_borders_(other.table_collapsed_borders_),
      table_column_geometries_(other.table_column_geometries_) {
  field_list_.ReserveInitialCapacity(other.field_list_.capacity());

  // Each field should be processed in order of FieldId to avoid vector
  // element insertions.

  SET_IF_EXISTS(kScrollableOverflow, scrollable_overflow, other);
  SET_IF_EXISTS(kBorders, borders, other);
  SET_IF_EXISTS(kScrollbar, scrollbar, other);
  SET_IF_EXISTS(kPadding, padding, other);
  SET_IF_EXISTS(kInflowBounds, inflow_bounds, other);
  CLONE_IF_EXISTS(kFrameSetLayoutData, frame_set_layout_data, other);
  SET_IF_EXISTS(kTableGridRect, table_grid_rect, other);
  CLONE_IF_EXISTS(kTableCollapsedBordersGeometry,
                  table_collapsed_borders_geometry, other);
  SET_IF_EXISTS(kTableCellColumnIndex, table_cell_column_index, other);
  SET_IF_EXISTS(kTableSectionStartRowIndex, table_section_start_row_index,
                other);
  SET_IF_EXISTS(kTableSectionRowOffsets, table_section_row_offsets, other);
  SET_IF_EXISTS(kPageName, page_name, other);
  SET_IF_EXISTS(kMargins, margins, other);

  DCHECK_EQ(field_list_.size(), other.field_list_.size());
}

#undef SET_IF_EXISTS
#undef CLONE_IF_EXISTS

PhysicalFragmentRareData::~PhysicalFragmentRareData() = default;

// RareField struct -----------------------------------------------------------

#define DISPATCH_BY_MEMBER_TYPE(FUNC)                                       \
  switch (type) {                                                           \
    FUNC(kScrollableOverflow, scrollable_overflow);                         \
    FUNC(kBorders, borders);                                                \
    FUNC(kScrollbar, scrollbar);                                            \
    FUNC(kPadding, padding);                                                \
    FUNC(kInflowBounds, inflow_bounds);                                     \
    FUNC(kFrameSetLayoutData, frame_set_layout_data);                       \
    FUNC(kTableGridRect, table_grid_rect);                                  \
    FUNC(kTableCollapsedBordersGeometry, table_collapsed_borders_geometry); \
    FUNC(kTableCellColumnIndex, table_cell_column_index);                   \
    FUNC(kTableSectionStartRowIndex, table_section_start_row_index);        \
    FUNC(kTableSectionRowOffsets, table_section_row_offsets);               \
    FUNC(kPageName, page_name);                                             \
    FUNC(kMargins, margins);                                                \
  }

#define CONSTRUCT_UNION_MEMBER(id, name) \
  case FieldId::id:                      \
    new (&name) decltype(name)();        \
    break

PhysicalFragmentRareData::RareField::RareField(
    PhysicalFragmentRareData::FieldId field_id)
    : type(field_id) {
  struct SameSizeAsRareField {
    union {
      std::unique_ptr<int> pointer;
      LayoutUnit units[4];
    };
    uint8_t type;
  };
  ASSERT_SIZE(RareField, SameSizeAsRareField);

  DISPATCH_BY_MEMBER_TYPE(CONSTRUCT_UNION_MEMBER);
}
#undef CONSTRUCT_UNION_MEMBER

// This invokes a copy constructor if the type has no move constructor.
#define MOVE_UNION_MEMBER(id, name)                    \
  case FieldId::id:                                    \
    new (&name) decltype(name)(std::move(other.name)); \
    break

PhysicalFragmentRareData::RareField::RareField(
    PhysicalFragmentRareData::RareField&& other)
    : type(other.type) {
  DISPATCH_BY_MEMBER_TYPE(MOVE_UNION_MEMBER);
}
#undef MOVE_UNION_MEMBER

#define DESTRUCT_UNION_MEMBER(id, name) \
  case FieldId::id: {                   \
    using NameType = decltype(name);    \
    name.~NameType();                   \
  } break

PhysicalFragmentRareData::RareField::~RareField() {
  DISPATCH_BY_MEMBER_TYPE(DESTRUCT_UNION_MEMBER);
}
#undef DESTRUCT_UNION_MEMBER

#undef DISPATCH_BY_MEMBER_TYPE

}  // namespace blink
