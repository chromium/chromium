// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"

namespace blink {

LayoutNGTableRow::LayoutNGTableRow(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

LayoutNGTableRow* LayoutNGTableRow::CreateAnonymousWithParent(
    const LayoutObject& parent) {
  scoped_refptr<const ComputedStyle> new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(), EDisplay::kTableRow);
  auto* new_row = MakeGarbageCollected<LayoutNGTableRow>(nullptr);
  new_row->SetDocumentForAnonymous(&parent.GetDocument());
  new_row->SetStyle(std::move(new_style));
  return new_row;
}

bool LayoutNGTableRow::IsEmpty() const {
  NOT_DESTROYED();
  return !FirstChild();
}

LayoutNGTableCell* LayoutNGTableRow::FirstCell() const {
  NOT_DESTROYED();
  return To<LayoutNGTableCell>(FirstChild());
}

LayoutNGTableCell* LayoutNGTableRow::LastCell() const {
  NOT_DESTROYED();
  return To<LayoutNGTableCell>(LastChild());
}

LayoutNGTableRow* LayoutNGTableRow::NextRow() const {
  NOT_DESTROYED();
  return To<LayoutNGTableRow>(NextSibling());
}

LayoutNGTableRow* LayoutNGTableRow::PreviousRow() const {
  NOT_DESTROYED();
  return To<LayoutNGTableRow>(PreviousSibling());
}

LayoutNGTableSection* LayoutNGTableRow::Section() const {
  NOT_DESTROYED();
  return To<LayoutNGTableSection>(Parent());
}

LayoutNGTable* LayoutNGTableRow::Table() const {
  NOT_DESTROYED();
  if (LayoutObject* section = Parent()) {
    if (LayoutObject* table = section->Parent())
      return To<LayoutNGTable>(table);
  }
  return nullptr;
}

void LayoutNGTableRow::AddChild(LayoutObject* child,
                                LayoutObject* before_child) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();

  if (!child->IsTableCell()) {
    LayoutObject* last = before_child;
    if (!last)
      last = LastCell();
    if (last && last->IsAnonymous() && last->IsTableCell() &&
        !last->IsBeforeOrAfterContent()) {
      LayoutBlockFlow* last_cell = To<LayoutBlockFlow>(last);
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

    // If before_child is inside an anonymous cell, insert into the cell.
    if (last && !last->IsTableCell() && last->Parent() &&
        last->Parent()->IsAnonymous() &&
        !last->Parent()->IsBeforeOrAfterContent()) {
      last->Parent()->AddChild(child, before_child);
      return;
    }

    auto* cell = LayoutNGTableCell::CreateAnonymousWithParent(*this);
    AddChild(cell, before_child);
    cell->AddChild(child);
    return;
  }

  if (before_child && before_child->Parent() != this)
    before_child = SplitAnonymousBoxesAroundChild(before_child);

  DCHECK(!before_child || before_child->IsTableCell());
  LayoutNGMixin<LayoutBlock>::AddChild(child, before_child);
}

void LayoutNGTableRow::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::RemoveChild(child);
}

void LayoutNGTableRow::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::WillBeRemovedFromTree();
}

void LayoutNGTableRow::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table()) {
    if ((old_style && !old_style->BorderVisuallyEqual(StyleRef())) ||
        (old_style && old_style->GetWritingDirection() !=
                          StyleRef().GetWritingDirection())) {
      table->GridBordersChanged();
    }
  }
  LayoutNGMixin<LayoutBlock>::StyleDidChange(diff, old_style);
}

LayoutBox* LayoutNGTableRow::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return CreateAnonymousWithParent(*parent);
}

LayoutBlock* LayoutNGTableRow::StickyContainer() const {
  NOT_DESTROYED();
  return Table();
}

#if DCHECK_IS_ON()
void LayoutNGTableRow::AddVisualOverflowFromBlockChildren() {
  NOT_DESTROYED();
  // This is computed in |NGPhysicalBoxFragment::ComputeSelfInkOverflow| and
  // that we should not reach here.
  NOTREACHED();
}
#endif

PositionWithAffinity LayoutNGTableRow::PositionForPoint(
    const PhysicalOffset& offset) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  // LayoutBlock::PositionForPoint is wrong for rows.
  return LayoutBox::PositionForPoint(offset);
}

unsigned LayoutNGTableRow::RowIndex() const {
  NOT_DESTROYED();
  unsigned index = 0;
  for (LayoutObject* child = Parent()->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child == this)
      return index;
    ++index;
  }
  NOTREACHED();
  return 0;
}

}  // namespace blink
