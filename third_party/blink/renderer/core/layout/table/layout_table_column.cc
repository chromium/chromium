// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/layout_table_column.h"

#include "third_party/blink/renderer/core/html/html_table_col_element.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/table_borders.h"
#include "third_party/blink/renderer/core/layout/table/table_break_token_data.h"
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

void LayoutTableColumn::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
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
  LayoutBox::StyleDidChange(diff, old_style, style_change_context);
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

PhysicalSize LayoutTableColumn::StitchedSize() const {
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

PhysicalOffset LayoutTableColumn::PhysicalLocation() const {
  NOT_DESTROYED();
  auto* table = Table();
  DCHECK(table);
  if (!table->PhysicalFragmentCount()) {
    // The tree may be dirty, and the table may not have been laid out even once
    // yet. Scroll anchoring does this, for instance.
    DCHECK(NeedsLayout());
    return PhysicalOffset();
  }

  LayoutTableColumn* parent_colgroup = nullptr;
  if (IsColumn()) {
    parent_colgroup = DynamicTo<LayoutTableColumn>(Parent());
    DCHECK(!parent_colgroup || parent_colgroup->IsColumnGroup());
  }

  const PhysicalBoxFragment& first_table_fragment =
      *table->GetPhysicalFragment(0);
  PhysicalOffset offset;

  ForAllSynthesizedFragments([&](const SynthesizedFragment& fragment) -> bool {
    offset = fragment.rect.offset;
    if (!parent_colgroup) {
      offset += fragment.table_fragment.OffsetFromRootFragmentationContext() -
                first_table_fragment.OffsetFromRootFragmentationContext();
    }
    // Stop walking. We only need the first "fragment".
    return false;
  });

  return offset;
}

PhysicalRect LayoutTableColumn::BoundingBoxRelativeToFirstFragment() const {
  NOT_DESTROYED();
  std::optional<PhysicalOffset> first_offset_from_fragmentation_context_root;
  PhysicalRect bounding_rect;

  ForAllSynthesizedFragments([&](const SynthesizedFragment& fragment) -> bool {
    PhysicalOffset offset =
        fragment.additional_offset_from_table_fragment + fragment.rect.offset +
        fragment.table_fragment.OffsetFromRootFragmentationContext();
    if (!first_offset_from_fragmentation_context_root) {
      first_offset_from_fragmentation_context_root = offset;
    }
    // Make offsets relative to the first fragment.
    offset -= *first_offset_from_fragmentation_context_root;
    PhysicalRect rect(offset, fragment.rect.size);
    bounding_rect.UniteEvenIfEmpty(rect);
    return true;
  });

  return bounding_rect;
}

void LayoutTableColumn::QuadsInAncestorInternal(
    Vector<gfx::QuadF>& quads,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  // Offset from the root fragmentation context to the first synthesized table
  // column fragment. When mapping to ancestors, it's all about the offsets from
  // the *first* fragment to the *first* container fragment, so any subsequent
  // fragment needs to convert their offsets so that they become relative to the
  // first fragment.
  std::optional<PhysicalOffset> first_offset_from_fragmentation_context_root;

  ForAllSynthesizedFragments([&](const SynthesizedFragment& fragment) -> bool {
    PhysicalOffset offset;
    if (!first_offset_from_fragmentation_context_root) {
      first_offset_from_fragmentation_context_root =
          fragment.additional_offset_from_table_fragment +
          fragment.rect.offset +
          fragment.table_fragment.OffsetFromRootFragmentationContext();
    } else {
      // Make the offset relative to the first fragment.
      PhysicalOffset offset_from_fragmentation_context_root =
          fragment.additional_offset_from_table_fragment +
          fragment.rect.offset +
          fragment.table_fragment.OffsetFromRootFragmentationContext();
      offset = offset_from_fragmentation_context_root -
               *first_offset_from_fragmentation_context_root;
    }
    PhysicalRect rect(offset, fragment.rect.size);
    quads.push_back(LocalRectToAncestorQuad(rect, ancestor, mode));
    return true;
  });

  if (quads.empty()) {
    // getClientRects() is expected to return a non-empty list if the element
    // has a layout box, but we may have failed to add any above if there are no
    // table sections (but still column or column group boxes).
    quads.push_back(LocalRectToAncestorQuad(PhysicalRect(), ancestor, mode));
  }
}

void LayoutTableColumn::ForAllSynthesizedFragments(
    base::FunctionRef<bool(const SynthesizedFragment&)> callback) const {
  NOT_DESTROYED();
  auto* table = Table();
  DCHECK(table);
  DCHECK_GT(table->PhysicalFragmentCount(), 0u);

  WritingDirectionMode writing_direction = StyleRef().GetWritingDirection();
  LayoutTableColumn* parent_colgroup = nullptr;
  if (IsColumn()) {
    parent_colgroup = DynamicTo<LayoutTableColumn>(Parent());
    DCHECK(!parent_colgroup || parent_colgroup->IsColumnGroup());
  }

  for (auto& fragment : table->PhysicalFragments()) {
    if (!fragment.TableColumnGeometries()) {
      // No table grid at all.
      return;
    }

    // If there was a table relayout, and this column box doesn't have a
    // corresponding column in the table anymore, the column_idx_ will not
    // have been updated. Therefore if it is greater or equal to the number of
    // table column geometries, or if the geometry at that index doesn't point
    // to this layout box, we return early.
    //
    // TODO(layout-dev): Removing this check doesn't break any tests. Need more
    // tests?
    if (column_idx_ >= fragment.TableColumnGeometries()->size()) {
      return;
    }

    const auto& geometry = (*fragment.TableColumnGeometries())[column_idx_];
    // TODO(layout-dev): Removing this check doesn't break any tests. Need more
    // tests?
    if (geometry.node.GetLayoutBox() != this) {
      return;
    }

    const TableBreakTokenData* table_break_token_data = nullptr;
    if (const BlockBreakToken* break_token = fragment.GetBreakToken()) {
      table_break_token_data =
          DynamicTo<TableBreakTokenData>(break_token->TokenData());
      if (table_break_token_data &&
          !table_break_token_data->has_entered_table_box) {
        // We haven't got to the table grid yet. No table columns here. Keep
        // looking.
        continue;
      }
    }

    // TableColumnGeometries use logical coordinates, which is a bit awkward
    // now.
    LogicalRect logical_rect;
    logical_rect.offset.inline_offset = geometry.inline_offset;
    logical_rect.size.inline_size = geometry.inline_size;

    WritingModeConverter table_converter(writing_direction, fragment.Size());
    // Unite all table sections in this table fragment to calculate the
    // block-size of the table column / table column group.
    LogicalRect sections_bounding_box;
    for (const PhysicalFragmentLink& child : fragment.Children()) {
      if (child->IsLayoutObjectDestroyedOrMoved()) {
        // This code may be run on a dirty layout tree. The scroll anchor code,
        // for instance, essentially always works on a dirty tree, and may
        // invoke LayoutTableColumn::PhysicalLocation(), so that we end up here.
        continue;
      }
      if (child->IsTableSection()) {
        LogicalRect section_rect = table_converter.ToLogical(
            PhysicalRect(child.offset, child->Size()));
        sections_bounding_box.Unite(section_rect);
      }
    }
    sections_bounding_box.offset.inline_offset =
        logical_rect.offset.inline_offset;
    sections_bounding_box.size.inline_size = logical_rect.size.inline_size;
    logical_rect.Unite(sections_bounding_box);

    PhysicalSize container_size = fragment.Size();
    PhysicalOffset additional_offset_from_table_fragment;
    if (parent_colgroup) {
      const auto& parent_geometry =
          (*fragment.TableColumnGeometries())[parent_colgroup->column_idx_];

      // Make the rectangle relative to the parent table-column-group, but keep
      // track of the offset that we subtract from the "fragment" rect by doing
      // so, to make it possible to calculate the table column offset relatively
      // to the root fragmentation context.
      LogicalOffset additional_offset(parent_geometry.inline_offset,
                                      logical_rect.offset.block_offset);
      logical_rect.offset.inline_offset -= parent_geometry.inline_offset;
      logical_rect.offset.block_offset = LayoutUnit();

      LogicalSize group_logical_size(parent_geometry.inline_size,
                                     sections_bounding_box.size.block_size);
      container_size = ToPhysicalSize(group_logical_size,
                                      writing_direction.GetWritingMode());
      additional_offset_from_table_fragment =
          table_converter.ToPhysical(additional_offset, container_size);
    } else {
      // Outer TableColumnGeometries (table-column-group, or table-column
      // without a table-column-group parent) are relative to the inline-start
      // edge of sections, so we have to add inline-start border/padding and
      // initial border-spacing manually.
      BoxStrut decorations = (fragment.Padding() + fragment.Borders())
                                 .ConvertToLogical(writing_direction);
      logical_rect.offset.inline_offset +=
          decorations.inline_start +
          table->StyleRef().TableBorderSpacing().inline_size;
    }

    WritingModeConverter container_converter(writing_direction, container_size);
    PhysicalRect rect = container_converter.ToPhysical(logical_rect);

    if (!callback(SynthesizedFragment(
            rect, additional_offset_from_table_fragment, fragment))) {
      return;
    }

    if (table_break_token_data && table_break_token_data->is_past_table_box) {
      // End of table grid. Only bottom captions to follow. No table columns
      // there.
      return;
    }
  }
}

}  // namespace blink
