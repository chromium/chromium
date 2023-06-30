// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_column.h"

#include "third_party/blink/renderer/core/html/html_table_col_element.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"

namespace blink {

LayoutNGTableColumn::LayoutNGTableColumn(Element* element)
    : LayoutBox(element) {
  UpdateFromElement();
}

void LayoutNGTableColumn::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  LayoutBox::Trace(visitor);
}

void LayoutNGTableColumn::StyleDidChange(StyleDifference diff,
                                         const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (diff.HasDifference()) {
    if (LayoutNGTable* table = Table()) {
      if (old_style && diff.NeedsNormalPaintInvalidation()) {
        // Regenerate table borders if needed
        if (!old_style->BorderVisuallyEqual(StyleRef()))
          table->GridBordersChanged();
        // Table paints column background. Tell table to repaint.
        if (StyleRef().HasBackground() || old_style->HasBackground())
          table->SetBackgroundNeedsFullPaintInvalidation();
      }
      if (diff.NeedsLayout()) {
        table->SetIntrinsicLogicalWidthsDirty();
        if (old_style &&
            NGTableTypes::CreateColumn(
                *old_style,
                /* default_inline_size */ absl::nullopt,
                table->StyleRef().IsFixedTableLayout()) !=
                NGTableTypes::CreateColumn(
                    StyleRef(), /* default_inline_size */ absl::nullopt,
                    table->StyleRef().IsFixedTableLayout())) {
          table->GridBordersChanged();
        }
      }
    }
  }
  LayoutBox::StyleDidChange(diff, old_style);
}

void LayoutNGTableColumn::ImageChanged(WrappedImagePtr, CanDeferInvalidation) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table()) {
    table->SetShouldDoFullPaintInvalidationWithoutLayoutChange(
        PaintInvalidationReason::kImage);
  }
}

void LayoutNGTableColumn::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBox::InsertedIntoTree();
  LayoutNGTable* table = Table();
  DCHECK(table);
  if (StyleRef().HasBackground())
    table->SetBackgroundNeedsFullPaintInvalidation();
  table->TableGridStructureChanged();
}

void LayoutNGTableColumn::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutBox::WillBeRemovedFromTree();
  LayoutNGTable* table = Table();
  DCHECK(table);
  if (StyleRef().HasBackground())
    table->SetBackgroundNeedsFullPaintInvalidation();
  table->TableGridStructureChanged();
}

bool LayoutNGTableColumn::IsChildAllowed(LayoutObject* child,
                                         const ComputedStyle& style) const {
  NOT_DESTROYED();
  return child->IsLayoutTableCol() && style.Display() == EDisplay::kTableColumn;
}

bool LayoutNGTableColumn::CanHaveChildren() const {
  NOT_DESTROYED();
  // <col> cannot have children.
  return IsColumnGroup();
}

void LayoutNGTableColumn::ClearNeedsLayoutForChildren() const {
  NOT_DESTROYED();
  LayoutObject* child = children_.FirstChild();
  while (child) {
    child->ClearNeedsLayout();
    child = child->NextSibling();
  }
}

LayoutNGTable* LayoutNGTableColumn::Table() const {
  NOT_DESTROYED();
  LayoutObject* table = Parent();
  if (table && !table->IsTable())
    table = table->Parent();
  if (table) {
    DCHECK(table->IsTable());
    return To<LayoutNGTable>(table);
  }
  return nullptr;
}

void LayoutNGTableColumn::UpdateFromElement() {
  NOT_DESTROYED();
  unsigned old_span = span_;
  if (const auto* tc = DynamicTo<HTMLTableColElement>(GetNode())) {
    span_ = tc->span();
  } else {
    span_ = 1;
  }
  if (span_ != old_span && Style() && Parent()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kAttributeChanged);
    if (LayoutNGTable* table = Table())
      table->GridBordersChanged();
  }
}

PhysicalSize LayoutNGTableColumn::Size() const {
  NOT_DESTROYED();
  if (!RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled()) {
    return frame_size_;
  }

  auto* table = Table();
  DCHECK(table);
  if (table->PhysicalFragmentCount() == 0) {
    return PhysicalSize();
  }

  WritingDirectionMode direction = StyleRef().GetWritingDirection();

  LogicalSize size;
  bool found_geometries = false;

  for (auto& fragment : table->PhysicalFragments()) {
    if (!found_geometries && fragment.TableColumnGeometries()) {
      // If there was a table relayout, and this column box doesn't have a
      // corresponding column in the table anymore, the column_idx_ will not
      // have been updated. Therefore if it is greater or equal to the number of
      // table column geometries, or if the geometry at that index doesn't point
      // to this layout box, we return early.
      if (column_idx_ >= fragment.TableColumnGeometries()->size()) {
        return PhysicalSize();
      }
      const auto& geometry = (*fragment.TableColumnGeometries())[column_idx_];
      if (geometry.node.GetLayoutBox() != this) {
        return PhysicalSize();
      }

      found_geometries = true;
      size.inline_size = geometry.inline_size;
      size.block_size -=
          table->StyleRef().TableBorderSpacing().block_size * 2 +
          fragment.Padding().ConvertToLogical(direction).BlockSum() +
          fragment.Borders().ConvertToLogical(direction).BlockSum();
    }

    size.block_size += fragment.TableGridRect().size.block_size;
  }

  return ToPhysicalSize(size, table->StyleRef().GetWritingMode());
}

LayoutPoint LayoutNGTableColumn::LocationInternal() const {
  NOT_DESTROYED();
  if (!RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled()) {
    return frame_location_;
  }

  auto* table = Table();
  DCHECK(table);
  if (table->PhysicalFragmentCount() == 0) {
    return LayoutPoint();
  }

  WritingDirectionMode direction = StyleRef().GetWritingDirection();
  LayoutNGTableColumn* parent_colgroup = nullptr;
  if (IsColumn()) {
    parent_colgroup = DynamicTo<LayoutNGTableColumn>(Parent());
    DCHECK(!parent_colgroup || parent_colgroup->IsColumnGroup());
  }

  LogicalOffset offset;
  LogicalSize size;
  LayoutUnit parent_colgroup_inline_size;
  bool found_geometries = false;

  for (auto& fragment : table->PhysicalFragments()) {
    if (!found_geometries && fragment.TableColumnGeometries()) {
      // If there was a table relayout, and this column box doesn't have a
      // corresponding column in the table anymore, the column_idx_ will not
      // have been updated. Therefore if it is greater or equal to the number of
      // table column geometries, or if the geometry at that index doesn't point
      // to this layout box, we return early.
      if (column_idx_ >= fragment.TableColumnGeometries()->size()) {
        return LayoutPoint();
      }
      const auto& geometry = (*fragment.TableColumnGeometries())[column_idx_];
      if (geometry.node.GetLayoutBox() != this) {
        return LayoutPoint();
      }

      found_geometries = true;
      offset.inline_offset = geometry.inline_offset;
      if (parent_colgroup) {
        const auto& parent_geometry =
            (*fragment.TableColumnGeometries())[parent_colgroup->column_idx_];
        offset.inline_offset -= parent_geometry.inline_offset;
        parent_colgroup_inline_size = parent_geometry.inline_size;
      }
      size.inline_size = geometry.inline_size;

      NGBoxStrut fragment_bp =
          (fragment.Padding() + fragment.Borders()).ConvertToLogical(direction);
      LogicalSize table_border_spacing = table->StyleRef().TableBorderSpacing();
      size.block_size -=
          table_border_spacing.block_size * 2 + fragment_bp.BlockSum();
      if (!parent_colgroup) {
        offset.inline_offset +=
            fragment_bp.inline_start + table_border_spacing.inline_size;
        offset.block_offset += fragment_bp.block_start +
                               table_border_spacing.block_size +
                               fragment.TableGridRect().offset.block_offset;
      }
    }

    size.block_size += fragment.TableGridRect().size.block_size;
  }

  PhysicalSize outer_size;
  if (!parent_colgroup) {
    outer_size = PhysicalSize(table->Size());
  } else {
    DCHECK_EQ(parent_colgroup->StyleRef().GetWritingDirection(), direction);
    outer_size = ToPhysicalSize(
        LogicalSize(parent_colgroup_inline_size, size.block_size),
        direction.GetWritingMode());
  }
  PhysicalSize inner_size = ToPhysicalSize(size, direction.GetWritingMode());
  return offset.ConvertToPhysical(direction, outer_size, inner_size)
      .ToLayoutPoint();
}

}  // namespace blink
