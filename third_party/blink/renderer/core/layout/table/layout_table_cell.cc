// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/table_constants.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/mathml/mathml_table_cell_element.h"
#include "third_party/blink/renderer/core/paint/table_cell_paint_invalidator.h"

namespace blink {

LayoutTableCell::LayoutTableCell(Element* element) : LayoutBlockFlow(element) {
  UpdateColAndRowSpanFlags();
}

LayoutTableCell* LayoutTableCell::CreateAnonymousWithParent(
    const LayoutObject& parent) {
  const ComputedStyle* new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(), EDisplay::kTableCell);
  auto* new_cell = MakeGarbageCollected<LayoutTableCell>(nullptr);
  new_cell->SetDocumentForAnonymous(&parent.GetDocument());
  new_cell->SetStyle(new_style);
  return new_cell;
}

void LayoutTableCell::InvalidateLayoutResultCacheAfterMeasure() const {
  NOT_DESTROYED();
  if (LayoutBox* row = ParentBox()) {
    DCHECK(row->IsTableRow());
    row->SetShouldSkipLayoutCache(true);
    if (LayoutBox* section = row->ParentBox()) {
      DCHECK(section->IsTableSection());
      section->SetShouldSkipLayoutCache(true);
    }
  }
}

LayoutUnit LayoutTableCell::BorderTop() const {
  NOT_DESTROYED();
  // TODO(1061423) Should return cell border, not fragment border.
  // To compute cell border, cell needs to know its starting row
  // and column, which are not available here.
  // PhysicalFragmentCount() > 0 check should not be necessary,
  // but it is because of TextAutosizer/ScrollAnchoring.
  if (Table()->HasCollapsedBorders() && PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().top;
  }
  return LayoutBlockFlow::BorderTop();
}

LayoutUnit LayoutTableCell::BorderBottom() const {
  NOT_DESTROYED();
  // TODO(1061423) Should return cell border, not fragment border.
  if (Table()->HasCollapsedBorders() && PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().bottom;
  }
  return LayoutBlockFlow::BorderBottom();
}

LayoutUnit LayoutTableCell::BorderLeft() const {
  NOT_DESTROYED();
  // TODO(1061423) Should return cell border, not fragment border.
  if (Table()->HasCollapsedBorders() && PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().left;
  }
  return LayoutBlockFlow::BorderLeft();
}

LayoutUnit LayoutTableCell::BorderRight() const {
  NOT_DESTROYED();
  // TODO(1061423) Should return cell border, not fragment border.
  if (Table()->HasCollapsedBorders() && PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().right;
  }
  return LayoutBlockFlow::BorderRight();
}

LayoutTableCell* LayoutTableCell::NextCell() const {
  NOT_DESTROYED();
  return To<LayoutTableCell>(NextSibling());
}

LayoutTableCell* LayoutTableCell::PreviousCell() const {
  NOT_DESTROYED();
  return To<LayoutTableCell>(PreviousSibling());
}

LayoutTableRow* LayoutTableCell::Row() const {
  NOT_DESTROYED();
  return To<LayoutTableRow>(Parent());
}

LayoutTableSection* LayoutTableCell::Section() const {
  NOT_DESTROYED();
  return To<LayoutTableSection>(Parent()->Parent());
}

LayoutTable* LayoutTableCell::Table() const {
  NOT_DESTROYED();
  if (LayoutObject* parent = Parent()) {
    if (LayoutObject* grandparent = parent->Parent()) {
      return To<LayoutTable>(grandparent->Parent());
    }
  }
  return nullptr;
}

void LayoutTableCell::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    if ((old_style && !old_style->BorderVisuallyEqual(StyleRef())) ||
        (old_style && old_style->GetWritingDirection() !=
                          StyleRef().GetWritingDirection())) {
      table->GridBordersChanged();
    }
  }
  LayoutBlockFlow::StyleDidChange(diff, old_style);
}

void LayoutTableCell::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->TableGridStructureChanged();
  }
  LayoutBlockFlow::WillBeRemovedFromTree();
}

void LayoutTableCell::ColSpanOrRowSpanChanged() {
  NOT_DESTROYED();
  UpdateColAndRowSpanFlags();
  if (LayoutTable* table = Table()) {
    table->SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kTableChanged);
    table->TableGridStructureChanged();
  }
}

LayoutBox* LayoutTableCell::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return CreateAnonymousWithParent(*parent);
}

LayoutBlock* LayoutTableCell::StickyContainer() const {
  NOT_DESTROYED();
  return Table();
}

void LayoutTableCell::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  NOT_DESTROYED();
  TableCellPaintInvalidator(*this, context).InvalidatePaint();
}

bool LayoutTableCell::BackgroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect) const {
  NOT_DESTROYED();
  // If this object has layer, the area of collapsed borders should be
  // transparent to expose the collapsed borders painted on the underlying
  // layer.
  if (HasLayer() && Table()->HasCollapsedBorders()) {
    return false;
  }
  return LayoutBlockFlow::BackgroundIsKnownToBeOpaqueInRect(local_rect);
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::RowIndex,
// verify behaviour is correct.
unsigned LayoutTableCell::RowIndex() const {
  NOT_DESTROYED();
  return To<LayoutTableRow>(Parent())->RowIndex();
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::CellForColumnAndRow,
// verify behaviour is correct.
unsigned LayoutTableCell::ResolvedRowSpan() const {
  NOT_DESTROYED();
  return ParsedRowSpan();
}

unsigned LayoutTableCell::AbsoluteColumnIndex() const {
  NOT_DESTROYED();
  if (PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->TableCellColumnIndex();
  }
  NOTREACHED_IN_MIGRATION() << "AbsoluteColumnIndex did not find cell";
  return 0;
}

unsigned LayoutTableCell::ColSpan() const {
  NOT_DESTROYED();
  if (!has_col_span_)
    return 1;
  return ParseColSpanFromDOM();
}

unsigned LayoutTableCell::ParseColSpanFromDOM() const {
  NOT_DESTROYED();
  if (const auto* cell_element = DynamicTo<HTMLTableCellElement>(GetNode())) {
    unsigned span = cell_element->colSpan();
    DCHECK_GE(span, kMinColSpan);
    DCHECK_LE(span, kMaxColSpan);
    return span;
  } else if (const auto* mathml_cell_element =
                 DynamicTo<MathMLTableCellElement>(GetNode())) {
    unsigned span = mathml_cell_element->colSpan();
    DCHECK_GE(span, kMinColSpan);
    DCHECK_LE(span, kMaxColSpan);
    return span;
  }
  return kDefaultRowSpan;
}

unsigned LayoutTableCell::ParseRowSpanFromDOM() const {
  NOT_DESTROYED();
  if (const auto* cell_element = DynamicTo<HTMLTableCellElement>(GetNode())) {
    unsigned span = cell_element->rowSpan();
    DCHECK_GE(span, kMinRowSpan);
    DCHECK_LE(span, kMaxRowSpan);
    return span;
  } else if (const auto* mathml_cell_element =
                 DynamicTo<MathMLTableCellElement>(GetNode())) {
    unsigned span = mathml_cell_element->rowSpan();
    DCHECK_GE(span, kMinRowSpan);
    DCHECK_LE(span, kMaxRowSpan);
    return span;
  }
  return kDefaultColSpan;
}

void LayoutTableCell::UpdateColAndRowSpanFlags() {
  NOT_DESTROYED();
  // Colspan or rowspan are rare, so we keep the values in DOM.
  has_col_span_ = ParseColSpanFromDOM() != kDefaultColSpan;
  has_rowspan_ = ParseRowSpanFromDOM() != kDefaultRowSpan;
}

}  // namespace blink
