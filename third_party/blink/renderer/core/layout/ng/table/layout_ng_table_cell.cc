// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"

#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/table_constants.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"
#include "third_party/blink/renderer/core/paint/ng/ng_table_cell_paint_invalidator.h"

namespace blink {

LayoutNGTableCell::LayoutNGTableCell(Element* element)
    : LayoutNGBlockFlowMixin<LayoutBlockFlow>(element) {
  UpdateColAndRowSpanFlags();
}

void LayoutNGTableCell::InvalidateLayoutResultCacheAfterMeasure() const {
  NOT_DESTROYED();
  if (LayoutBox* row = ParentBox()) {
    DCHECK(row->IsTableRow());
    row->ClearLayoutResults();
    if (LayoutBox* section = row->ParentBox()) {
      DCHECK(section->IsTableSection());
      section->ClearLayoutResults();
    }
  }
}

LayoutRectOutsets LayoutNGTableCell::BorderBoxOutsets() const {
  NOT_DESTROYED();
  // TODO(1061423) This function should not be called before layout.
  // ScrollAnchor::Examine does. Example trigger:
  // ScrollTimelineTest.TimelineInvalidationWhenScrollerDisplayPropertyChanges
  // DCHECK_GE(PhysicalFragmentCount(), 0u);
  if (PhysicalFragmentCount() > 0)
    return GetPhysicalFragment(0)->Borders().ToLayoutRectOutsets();
  return LayoutNGBlockFlowMixin<LayoutBlockFlow>::BorderBoxOutsets();
}

LayoutUnit LayoutNGTableCell::BorderTop() const {
  NOT_DESTROYED();
  // TODO(1061423) Should return cell border, not fragment border.
  // To compute cell border, cell needs to know its starting row
  // and column, which are not available here.
  // PhysicalFragmentCount() > 0 check should not be necessary,
  // but it is because of TextAutosizer/ScrollAnchoring.
  if (Table()->ShouldCollapseBorders() && PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().top;
  }
  return LayoutNGBlockFlowMixin<LayoutBlockFlow>::BorderTop();
}

LayoutUnit LayoutNGTableCell::BorderBottom() const {
  NOT_DESTROYED();
  // TODO(1061423) Should return cell border, not fragment border.
  if (Table()->ShouldCollapseBorders() && PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().bottom;
  }
  return LayoutNGBlockFlowMixin<LayoutBlockFlow>::BorderBottom();
}

LayoutUnit LayoutNGTableCell::BorderLeft() const {
  NOT_DESTROYED();
  // TODO(1061423) Should return cell border, not fragment border.
  if (Table()->ShouldCollapseBorders() && PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().left;
  }
  return LayoutNGBlockFlowMixin<LayoutBlockFlow>::BorderLeft();
}

LayoutUnit LayoutNGTableCell::BorderRight() const {
  NOT_DESTROYED();
  // TODO(1061423) Should return cell border, not fragment border.
  if (Table()->ShouldCollapseBorders() && PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->Borders().right;
  }
  return LayoutNGBlockFlowMixin<LayoutBlockFlow>::BorderRight();
}

LayoutNGTable* LayoutNGTableCell::Table() const {
  NOT_DESTROYED();
  if (LayoutObject* parent = Parent()) {
    if (LayoutObject* grandparent = parent->Parent()) {
      return To<LayoutNGTable>(grandparent->Parent());
    }
  }
  return nullptr;
}

void LayoutNGTableCell::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }
  UpdateInFlowBlockLayout();
}

void LayoutNGTableCell::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table()) {
    if ((old_style && !old_style->BorderVisuallyEqual(StyleRef())) ||
        (old_style && old_style->GetWritingDirection() !=
                          StyleRef().GetWritingDirection())) {
      table->GridBordersChanged();
    }
  }
  LayoutNGBlockFlowMixin<LayoutBlockFlow>::StyleDidChange(diff, old_style);
}

void LayoutNGTableCell::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
  LayoutNGMixin<LayoutBlockFlow>::WillBeRemovedFromTree();
}

void LayoutNGTableCell::ColSpanOrRowSpanChanged() {
  NOT_DESTROYED();
  UpdateColAndRowSpanFlags();
  if (LayoutNGTable* table = Table()) {
    table->SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kTableChanged);
    table->TableGridStructureChanged();
  }
}

LayoutBox* LayoutNGTableCell::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return LayoutObjectFactory::CreateAnonymousTableCellWithParent(*parent);
}

LayoutBlock* LayoutNGTableCell::StickyContainer() const {
  NOT_DESTROYED();
  return Table();
}

void LayoutNGTableCell::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  NOT_DESTROYED();
  NGTableCellPaintInvalidator(*this, context).InvalidatePaint();
}

bool LayoutNGTableCell::BackgroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect) const {
  NOT_DESTROYED();
  // If this object has layer, the area of collapsed borders should be
  // transparent to expose the collapsed borders painted on the underlying
  // layer.
  if (HasLayer() && Table()->ShouldCollapseBorders())
    return false;
  return LayoutNGBlockFlowMixin<
      LayoutBlockFlow>::BackgroundIsKnownToBeOpaqueInRect(local_rect);
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::RowIndex,
// verify behaviour is correct.
unsigned LayoutNGTableCell::RowIndex() const {
  NOT_DESTROYED();
  return To<LayoutNGTableRow>(Parent())->RowIndex();
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::CellForColumnAndRow,
// verify behaviour is correct.
unsigned LayoutNGTableCell::ResolvedRowSpan() const {
  NOT_DESTROYED();
  return ParsedRowSpan();
}

unsigned LayoutNGTableCell::AbsoluteColumnIndex() const {
  NOT_DESTROYED();
  if (PhysicalFragmentCount() > 0) {
    return GetPhysicalFragment(0)->TableCellColumnIndex();
  }
  NOTREACHED() << "AbsoluteColumnIndex did not find cell";
  return 0;
}

unsigned LayoutNGTableCell::ColSpan() const {
  NOT_DESTROYED();
  if (!has_col_span_)
    return 1;
  return ParseColSpanFromDOM();
}

unsigned LayoutNGTableCell::ParseColSpanFromDOM() const {
  NOT_DESTROYED();
  if (const auto* cell_element = DynamicTo<HTMLTableCellElement>(GetNode())) {
    unsigned span = cell_element->colSpan();
    DCHECK_GE(span, kMinColSpan);
    DCHECK_LE(span, kMaxColSpan);
    return span;
  }
  return kDefaultRowSpan;
}

unsigned LayoutNGTableCell::ParseRowSpanFromDOM() const {
  NOT_DESTROYED();
  if (const auto* cell_element = DynamicTo<HTMLTableCellElement>(GetNode())) {
    unsigned span = cell_element->rowSpan();
    DCHECK_GE(span, kMinRowSpan);
    DCHECK_LE(span, kMaxRowSpan);
    return span;
  }
  return kDefaultColSpan;
}

void LayoutNGTableCell::UpdateColAndRowSpanFlags() {
  NOT_DESTROYED();
  // Colspan or rowspan are rare, so we keep the values in DOM.
  has_col_span_ = ParseColSpanFromDOM() != kDefaultColSpan;
  has_rowspan_ = ParseRowSpanFromDOM() != kDefaultRowSpan;
}

LayoutNGTableInterface* LayoutNGTableCell::TableInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableInterface>(Parent()->Parent()->Parent());
}

LayoutNGTableCellInterface* LayoutNGTableCell::NextCellInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableCellInterface>(LayoutObject::NextSibling());
}

LayoutNGTableCellInterface* LayoutNGTableCell::PreviousCellInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableCellInterface>(
      LayoutObject::PreviousSibling());
}

LayoutNGTableRowInterface* LayoutNGTableCell::RowInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableRowInterface>(Parent());
}

LayoutNGTableSectionInterface* LayoutNGTableCell::SectionInterface() const {
  NOT_DESTROYED();
  return ToInterface<LayoutNGTableSectionInterface>(Parent()->Parent());
}

}  // namespace blink
