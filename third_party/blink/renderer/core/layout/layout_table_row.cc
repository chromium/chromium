/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013
*                Apple Inc.
 *               All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/layout_table_row.h"

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/subtree_layout_scope.h"
#include "third_party/blink/renderer/core/paint/table_row_painter.h"

namespace blink {

LayoutTableRow::LayoutTableRow(Element* element)
    : LayoutTableBoxComponent(element), row_index_(kUnsetRowIndex) {
  // init LayoutObject attributes
  SetInline(false);  // our object is not Inline
}

void LayoutTableRow::WillBeRemovedFromTree() {
  LayoutTableBoxComponent::WillBeRemovedFromTree();

  Section()->SetNeedsCellRecalc();
}

LayoutNGTableCellInterface* LayoutTableRow::FirstCellInterface() const {
  return FirstCell();
}

LayoutNGTableCellInterface* LayoutTableRow::LastCellInterface() const {
  return LastCell();
}

void LayoutTableRow::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  DCHECK_EQ(StyleRef().Display(), EDisplay::kTableRow);

  LayoutTableBoxComponent::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();

  if (!old_style)
    return;

  if (Section() && StyleRef().LogicalHeight() != old_style->LogicalHeight())
    Section()->RowLogicalHeightChanged(this);

  if (!Parent())
    return;
  LayoutTable* table = Table();
  if (!table)
    return;

  LayoutTableBoxComponent::InvalidateCollapsedBordersOnStyleChange(
      *this, *table, diff, *old_style);

  if (LayoutTableBoxComponent::DoCellsHaveDirtyWidth(*this, *table, diff,
                                                     *old_style)) {
    // If the border width changes on a row, we need to make sure the cells in
    // the row know to lay out again.
    // This only happens when borders are collapsed, since they end up affecting
    // the border sides of the cell itself.
    for (LayoutBox* child_box = FirstChildBox(); child_box;
         child_box = child_box->NextSiblingBox()) {
      if (!child_box->IsTableCell())
        continue;
      // TODO(dgrogan) Add a web test showing that SetChildNeedsLayout is
      // needed instead of SetNeedsLayout.
      child_box->SetChildNeedsLayout();
      child_box->SetPreferredLogicalWidthsDirty(kMarkOnlyThis);
    }
    // Most table componenents can rely on LayoutObject::styleDidChange
    // to mark the container chain dirty. But LayoutTableSection seems
    // to never clear its dirty bit, which stops the propagation. So
    // anything under LayoutTableSection has to restart the propagation
    // at the table.
    // TODO(dgrogan): Make LayoutTableSection clear its dirty bit.
    table->SetPreferredLogicalWidthsDirty();
  }

  // When a row gets collapsed or uncollapsed, it's necessary to check all the
  // rows to find any cell that may span the current row.
  if ((old_style->Visibility() == EVisibility::kCollapse) !=
      (StyleRef().Visibility() == EVisibility::kCollapse)) {
    for (LayoutTableRow* row = Section()->FirstRow(); row;
         row = row->NextRow()) {
      for (LayoutTableCell* cell = row->FirstCell(); cell;
           cell = cell->NextCell()) {
        if (!cell->IsSpanningCollapsedRow())
          continue;
        unsigned rowIndex = RowIndex();
        unsigned spanStart = cell->RowIndex();
        unsigned spanEnd = spanStart + cell->ResolvedRowSpan();
        if (spanStart <= rowIndex && rowIndex <= spanEnd)
          cell->SetCellChildrenNeedLayout();
      }
    }
  }
}

void LayoutTableRow::AddChild(LayoutObject* child, LayoutObject* before_child) {
  if (!child->IsTableCell()) {
    LayoutObject* last = before_child;
    if (!last)
      last = LastCell();
    if (last && last->IsAnonymous() && last->IsTableCell() &&
        !last->IsBeforeOrAfterContent()) {
      LayoutTableCell* last_cell = To<LayoutTableCell>(last);
      if (before_child == last_cell)
        before_child = last_cell->FirstChild();
      last_cell->AddChild(child, before_child);
      return;
    }

    if (before_child && !before_child->IsAnonymous() &&
        before_child->Parent() == this) {
      LayoutObject* cell = before_child->PreviousSibling();
      if (cell && cell->IsTableCell() && cell->IsAnonymous()) {
        cell->AddChild(child);
        return;
      }
    }

    // If beforeChild is inside an anonymous cell, insert into the cell.
    if (last && !last->IsTableCell() && last->Parent() &&
        last->Parent()->IsAnonymous() &&
        !last->Parent()->IsBeforeOrAfterContent()) {
      last->Parent()->AddChild(child, before_child);
      return;
    }

    LayoutTableCell* cell = LayoutTableCell::CreateAnonymousWithParent(this);
    AddChild(cell, before_child);
    cell->AddChild(child);
    return;
  }

  if (before_child && before_child->Parent() != this)
    before_child = SplitAnonymousBoxesAroundChild(before_child);

  LayoutTableCell* cell = To<LayoutTableCell>(child);

  DCHECK(!before_child || before_child->IsTableCell());
  LayoutTableBoxComponent::AddChild(cell, before_child);

  // Generated content can result in us having a null section so make sure to
  // null check our parent.
  if (Parent()) {
    Section()->AddCell(cell, this);
    // When borders collapse, adding a cell can affect the the width of
    // neighboring cells.
    LayoutTable* enclosing_table = Table();
    if (enclosing_table && enclosing_table->ShouldCollapseBorders()) {
      enclosing_table->InvalidateCollapsedBorders();
      if (LayoutTableCell* previous_cell = cell->PreviousCell()) {
        previous_cell->SetNeedsLayoutAndPrefWidthsRecalc(
            layout_invalidation_reason::kTableChanged);
      }
      if (LayoutTableCell* next_cell = cell->NextCell()) {
        next_cell->SetNeedsLayoutAndPrefWidthsRecalc(
            layout_invalidation_reason::kTableChanged);
      }
    }
  }

  if (before_child || NextRow() || !cell->ParsedRowSpan())
    Section()->SetNeedsCellRecalc();
}

void LayoutTableRow::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);
  bool paginated = View()->GetLayoutState()->IsPaginated();

  for (LayoutTableCell* cell = FirstCell(); cell; cell = cell->NextCell()) {
    SubtreeLayoutScope layouter(*cell);
    cell->SetLogicalTop(LogicalTop());
    if (!cell->NeedsLayout())
      Section()->MarkChildForPaginationRelayoutIfNeeded(*cell, layouter);
    if (cell->NeedsLayout()) {
      // If we are laying out the cell's children clear its intrinsic
      // padding so it doesn't skew the position of the content.
      if (cell->CellChildrenNeedLayout())
        cell->ClearIntrinsicPadding();
      cell->UpdateLayout();
    }
    if (paginated)
      Section()->UpdateFragmentationInfoForChild(*cell);
  }

  ClearLayoutOverflow();
  // We do not call addOverflowFromCell here. The cell are laid out to be
  // measured above and will be sized correctly in a follow-up phase.

  // We only ever need to issue paint invalidations if our cells didn't, which
  // means that they didn't need layout, so we know that our bounds didn't
  // change. This code is just making up for the fact that we did not invalidate
  // paints in setStyle() because we had a layout hint.
  if (SelfNeedsLayout()) {
    for (LayoutTableCell* cell = FirstCell(); cell; cell = cell->NextCell()) {
      // FIXME: Is this needed when issuing paint invalidations after layout?
      cell->SetShouldDoFullPaintInvalidation();
    }
  }

  // LayoutTableSection::layoutRows will set our logical height and width later,
  // so it calls updateLayerTransform().
  ClearNeedsLayout();
}

// Hit Testing
bool LayoutTableRow::NodeAtPoint(HitTestResult& result,
                                 const HitTestLocation& hit_test_location,
                                 const PhysicalOffset& accumulated_offset,
                                 HitTestAction action) {
  // The row and the cells are all located in the section.
  const auto* section = Section();
  PhysicalOffset section_accumulated_offset =
      accumulated_offset - PhysicalLocation(section);

  // Table rows cannot ever be hit tested.  Effectively they do not exist.
  // Just forward to our children always.
  for (LayoutTableCell* cell = LastCell(); cell; cell = cell->PreviousCell()) {
    if (cell->HasSelfPaintingLayer())
      continue;
    PhysicalOffset cell_accumulated_offset =
        section_accumulated_offset + cell->PhysicalLocation(section);
    if (cell->NodeAtPoint(result, hit_test_location, cell_accumulated_offset,
                          action)) {
      UpdateHitTestResult(
          result, hit_test_location.Point() - section_accumulated_offset);
      return true;
    }
  }

  return false;
}

LayoutBox::PaginationBreakability LayoutTableRow::GetPaginationBreakability()
    const {
  PaginationBreakability breakability =
      LayoutTableBoxComponent::GetPaginationBreakability();
  if (breakability == kAllowAnyBreaks) {
    // Even if the row allows us to break inside, we will want to prevent that
    // if we have a header group that wants to appear at the top of each page.
    if (const LayoutTableSection* header = Table()->Header())
      breakability = header->GetPaginationBreakability();
  }
  return breakability;
}

void LayoutTableRow::Paint(const PaintInfo& paint_info) const {
  TableRowPainter(*this).Paint(paint_info);
}

LayoutTableRow* LayoutTableRow::CreateAnonymous(Document* document) {
  LayoutTableRow* layout_object = new LayoutTableRow(nullptr);
  layout_object->SetDocumentForAnonymous(document);
  return layout_object;
}

LayoutTableRow* LayoutTableRow::CreateAnonymousWithParent(
    const LayoutObject* parent) {
  LayoutTableRow* new_row =
      LayoutTableRow::CreateAnonymous(&parent->GetDocument());
  scoped_refptr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(parent->StyleRef(),
                                                     EDisplay::kTableRow);
  new_row->SetStyle(std::move(new_style));
  return new_row;
}

void LayoutTableRow::ComputeLayoutOverflow() {
  ClearLayoutOverflow();
  for (LayoutTableCell* cell = FirstCell(); cell; cell = cell->NextCell())
    AddLayoutOverflowFromCell(cell);
}

void LayoutTableRow::RecalcVisualOverflow() {
  unsigned n_cols = Section()->NumCols(RowIndex());
  for (unsigned c = 0; c < n_cols; c++) {
    auto* cell = Section()->OriginatingCellAt(RowIndex(), c);
    if (!cell)
      continue;
    if (!cell->HasSelfPaintingLayer())
      cell->RecalcVisualOverflow();
  }

  ComputeVisualOverflow();
}

void LayoutTableRow::ComputeVisualOverflow() {
  const auto& old_visual_rect = VisualOverflowRect();
  ClearVisualOverflow();
  AddVisualEffectOverflow();

  for (LayoutTableCell* cell = FirstCell(); cell; cell = cell->NextCell())
    AddVisualOverflowFromCell(cell);
  if (old_visual_rect != VisualOverflowRect()) {
    SetShouldCheckForPaintInvalidation();
  }
}

void LayoutTableRow::AddLayoutOverflowFromCell(const LayoutTableCell* cell) {
  LayoutRect cell_layout_overflow_rect =
      cell->LayoutOverflowRectForPropagation(this);

  // The cell and the row share the section's coordinate system. However
  // the visual overflow should be determined in the coordinate system of
  // the row, that's why we shift the rects by cell_row_offset below.
  LayoutSize cell_row_offset = cell->Location() - Location();

  cell_layout_overflow_rect.Move(cell_row_offset);
  AddLayoutOverflow(cell_layout_overflow_rect);
}

void LayoutTableRow::AddVisualOverflowFromCell(const LayoutTableCell* cell) {
  // Note: we include visual overflow of even self-painting cells,
  // because the row needs to expand to contain their area in order to paint
  // background and collapsed borders. This is different than any other
  // LayoutObject subtype.

  // Table row paints its background behind cells. If the cell spans multiple
  // rows, the row's visual rect should be expanded to cover the cell.
  // Here don't check background existence to avoid requirement to invalidate
  // overflow on change of background existence.
  if (cell->ResolvedRowSpan() > 1) {
    LayoutRect cell_background_rect = cell->FrameRect();
    cell_background_rect.MoveBy(-Location());
    AddSelfVisualOverflow(cell_background_rect);
  }

  // The cell and the row share the section's coordinate system. However
  // the visual overflow should be determined in the coordinate system of
  // the row, that's why we shift the rects by cell_row_offset below.
  LayoutSize cell_row_offset = cell->Location() - Location();

  // Let the row's self visual overflow cover the cell's whole collapsed
  // borders. This ensures correct raster invalidation on row border style
  // change.
  if (const auto* collapsed_borders = cell->GetCollapsedBorderValues()) {
    LayoutRect collapsed_border_rect =
        cell->RectForOverflowPropagation(collapsed_borders->LocalVisualRect());
    collapsed_border_rect.Move(cell_row_offset);
    AddSelfVisualOverflow(collapsed_border_rect);
  }

  LayoutRect cell_visual_overflow_rect =
      cell->VisualOverflowRectForPropagation();
  cell_visual_overflow_rect.Move(cell_row_offset);
  AddContentsVisualOverflow(cell_visual_overflow_rect);
}

bool LayoutTableRow::PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
  return LayoutTableBoxComponent::
             PaintedOutputOfObjectHasNoEffectRegardlessOfSize() &&
         // Row paints collapsed borders.
         !Table()->HasCollapsedBorders();
}

}  // namespace blink
