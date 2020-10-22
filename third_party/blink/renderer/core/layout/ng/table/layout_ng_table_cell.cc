// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"

#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/table_constants.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"
#include "third_party/blink/renderer/core/style/border_edge.h"

namespace blink {

LayoutNGTableCell::LayoutNGTableCell(Element* element)
    : LayoutNGBlockFlowMixin<LayoutBlockFlow>(element) {
  UpdateColAndRowSpanFlags();
}

void LayoutNGTableCell::InvalidateLayoutResultCacheAfterMeasure() const {
  if (LayoutBox* row = ParentBox()) {
    DCHECK(row->IsTableRow());
    row->ClearLayoutResults();
    if (LayoutBox* section = row->ParentBox()) {
      DCHECK(section->IsTableSection());
      section->ClearLayoutResults();
    }
  }
}

LayoutNGTable* LayoutNGTableCell::Table() const {
  if (LayoutObject* parent = Parent()) {
    if (LayoutObject* grandparent = parent->Parent()) {
      return To<LayoutNGTable>(grandparent->Parent());
    }
  }
  return nullptr;
}

void LayoutNGTableCell::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }
  UpdateInFlowBlockLayout();
}

void LayoutNGTableCell::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  if (LayoutNGTable* table = Table()) {
    if (NGTableBorders::HasBorder(old_style) ||
        NGTableBorders::HasBorder(Style()))
      table->GridBordersChanged();
  }
  LayoutNGBlockFlowMixin<LayoutBlockFlow>::StyleDidChange(diff, old_style);
}

void LayoutNGTableCell::ColSpanOrRowSpanChanged() {
  // TODO(atotic) Invalidate layout?
  UpdateColAndRowSpanFlags();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
}

LayoutBox* LayoutNGTableCell::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  return LayoutObjectFactory::CreateAnonymousTableCellWithParent(*parent);
}

Length LayoutNGTableCell::StyleOrColLogicalWidth() const {
  // TODO(atotic) TablesNG cannot easily get col width before layout.
  return StyleRef().LogicalWidth();
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::RowIndex,
// verify behaviour is correct.
unsigned LayoutNGTableCell::RowIndex() const {
  return To<LayoutNGTableRow>(Parent())->RowIndex();
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::CellForColumnAndRow,
// verify behaviour is correct.
unsigned LayoutNGTableCell::ResolvedRowSpan() const {
  return ParsedRowSpan();
}

unsigned LayoutNGTableCell::AbsoluteColumnIndex() const {
  if (PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->TableCellColumnIndex();
  }
  NOTREACHED() << "AbsoluteColumnIndex did not find cell";
  return 0;
}

unsigned LayoutNGTableCell::ColSpan() const {
  if (!has_col_span_)
    return 1;
  return ParseColSpanFromDOM();
}

unsigned LayoutNGTableCell::ParseColSpanFromDOM() const {
  if (const auto* cell_element = DynamicTo<HTMLTableCellElement>(GetNode())) {
    unsigned span = cell_element->colSpan();
    DCHECK_GE(span, kMinColSpan);
    DCHECK_LE(span, kMaxColSpan);
    return span;
  }
  return kDefaultRowSpan;
}

unsigned LayoutNGTableCell::ParseRowSpanFromDOM() const {
  if (const auto* cell_element = DynamicTo<HTMLTableCellElement>(GetNode())) {
    unsigned span = cell_element->rowSpan();
    DCHECK_GE(span, kMinRowSpan);
    DCHECK_LE(span, kMaxRowSpan);
    return span;
  }
  return kDefaultColSpan;
}

void LayoutNGTableCell::UpdateColAndRowSpanFlags() {
  // Colspan or rowspan are rare, so we keep the values in DOM.
  has_col_span_ = ParseColSpanFromDOM() != kDefaultColSpan;
  has_rowspan_ = ParseRowSpanFromDOM() != kDefaultRowSpan;
}

LayoutNGTableInterface* LayoutNGTableCell::TableInterface() const {
  return ToInterface<LayoutNGTableInterface>(Parent()->Parent()->Parent());
}

LayoutNGTableCellInterface* LayoutNGTableCell::NextCellInterface() const {
  return ToInterface<LayoutNGTableCellInterface>(LayoutObject::NextSibling());
}

LayoutNGTableCellInterface* LayoutNGTableCell::PreviousCellInterface() const {
  return ToInterface<LayoutNGTableCellInterface>(
      LayoutObject::PreviousSibling());
}

LayoutNGTableRowInterface* LayoutNGTableCell::RowInterface() const {
  return ToInterface<LayoutNGTableRowInterface>(Parent());
}

LayoutNGTableSectionInterface* LayoutNGTableCell::SectionInterface() const {
  return ToInterface<LayoutNGTableSectionInterface>(Parent()->Parent());
}

}  // namespace blink
