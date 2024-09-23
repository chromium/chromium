// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"

#include "third_party/blink/renderer/core/html/html_table_col_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/table_borders.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_algorithm_types.h"

namespace blink {

namespace {

// Returns whether any of the given table's columns have backgrounds, even if
// they don't have any associated cells (unlike
// `LayoutTable::HasBackgroundForPaint`). Used to know whether the table
// background should be invalidated when some column span changes.
bool TableHasColumnsWithBackground(LayoutTable* table) {
  TableGroupedChildren grouped_children(BlockNode{table});
  for (const auto& column : grouped_children.columns) {
    if (column.Style().HasBackground()) {
      return true;
    }

    // Iterate through a colgroup's children.
    if (column.IsTableColgroup()) {
      LayoutInputNode node = column.FirstChild();
      while (node) {
        DCHECK(node.IsTableCol());
        if (node.Style().HasBackground()) {
          return true;
        }
        node = node.NextSibling();
      }
    }
  }

  return false;
}

}  // namespace

LayoutTableColumn::LayoutTableColumn(Element* element) : LayoutBox(element) {
  UpdateFromElement();
}

void LayoutTableColumn::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  LayoutBox::Trace(visitor);
}

void LayoutTableColumn::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (diff.HasDifference()) {
    if (LayoutTable* table = Table()) {
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
            TableTypes::CreateColumn(*old_style,
                                     /* default_inline_size */ std::nullopt,
                                     table->StyleRef().IsFixedTableLayout()) !=
                TableTypes::CreateColumn(
                    StyleRef(), /* default_inline_size */ std::nullopt,
                    table->StyleRef().IsFixedTableLayout())) {
          table->GridBordersChanged();
        }
      }
    }
  }
  LayoutBox::StyleDidChange(diff, old_style);
}

void LayoutTableColumn::ImageChanged(WrappedImagePtr, CanDeferInvalidation) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->SetShouldDoFullPaintInvalidationWithoutLayoutChange(
        PaintInvalidationReason::kImage);
  }
}

void LayoutTableColumn::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBox::InsertedIntoTree();
  LayoutTable* table = Table();
  DCHECK(table);
  if (StyleRef().HasBackground())
    table->SetBackgroundNeedsFullPaintInvalidation();
  table->TableGridStructureChanged();
}

void LayoutTableColumn::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutBox::WillBeRemovedFromTree();
  LayoutTable* table = Table();
  DCHECK(table);
  if (StyleRef().HasBackground())
    table->SetBackgroundNeedsFullPaintInvalidation();
  table->TableGridStructureChanged();
}

bool LayoutTableColumn::IsChildAllowed(LayoutObject* child,
                                       const ComputedStyle& style) const {
  NOT_DESTROYED();
  return child->IsLayoutTableCol() && style.Display() == EDisplay::kTableColumn;
}

bool LayoutTableColumn::CanHaveChildren() const {
  NOT_DESTROYED();
  // <col> cannot have children.
  return IsColumnGroup();
}

void LayoutTableColumn::ClearNeedsLayoutForChildren() const {
  NOT_DESTROYED();
  LayoutObject* child = children_.FirstChild();
  while (child) {
    child->ClearNeedsLayout();
    child = child->NextSibling();
  }
}

LayoutTable* LayoutTableColumn::Table() const {
  NOT_DESTROYED();
  LayoutObject* table = Parent();
  if (table && !table->IsTable())
    table = table->Parent();
  if (table) {
    DCHECK(table->IsTable());
    return To<LayoutTable>(table);
  }
  return nullptr;
}

void LayoutTableColumn::UpdateFromElement() {
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
    if (LayoutTable* table = Table()) {
      table->GridBordersChanged();
      if (Style()->HasBackground() || TableHasColumnsWithBackground(table)) {
        table->SetBackgroundNeedsFullPaintInvalidation();
      }
    }
  }
}

PhysicalSize LayoutTableColumn::Size() const {
  NOT_DESTROYED();
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
      size.block_size -= table->StyleRef().TableBorderSpacing().block_size * 2;
    }

    size.block_size +=
        fragment.TableGridRect().size.block_size -
        (fragment.Padding().ConvertToLogical(direction).BlockSum() +
         fragment.Borders().ConvertToLogical(direction).BlockSum());
  }

  return ToPhysicalSize(size, table->StyleRef().GetWritingMode());
}

LayoutPoint LayoutTableColumn::LocationInternal() const {
  NOT_DESTROYED();
  auto* table = Table();
  DCHECK(table);
  if (table->PhysicalFragmentCount() == 0) {
    return LayoutPoint();
  }

  WritingDirectionMode direction = StyleRef().GetWritingDirection();
  LayoutTableColumn* parent_colgroup = nullptr;
  if (IsColumn()) {
    parent_colgroup = DynamicTo<LayoutTableColumn>(Parent());
    DCHECK(!parent_colgroup || parent_colgroup->IsColumnGroup());
  }

  LogicalOffset offset;
  LogicalSize size;
  LayoutUnit parent_colgroup_inline_size;
  bool found_geometries = false;

  for (auto& fragment : table->PhysicalFragments()) {
    BoxStrut decorations =
        (fragment.Padding() + fragment.Borders()).ConvertToLogical(direction);
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

      LogicalSize table_border_spacing = table->StyleRef().TableBorderSpacing();
      size.block_size -= table_border_spacing.block_size * 2;
      if (!parent_colgroup) {
        offset.inline_offset +=
            decorations.inline_start + table_border_spacing.inline_size;
        offset.block_offset += decorations.block_start +
                               table_border_spacing.block_size +
                               fragment.TableGridRect().offset.block_offset;
      }
    }

    size.block_size +=
        fragment.TableGridRect().size.block_size - decorations.BlockSum();
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
