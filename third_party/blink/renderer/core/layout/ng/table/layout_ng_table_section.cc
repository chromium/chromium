// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"

namespace blink {

LayoutNGTableSection::LayoutNGTableSection(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

bool LayoutNGTableSection::IsEmpty() const {
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (!To<LayoutNGTableRow>(child)->IsEmpty())
      return false;
  }
  return true;
}

LayoutNGTable* LayoutNGTableSection::Table() const {
  return To<LayoutNGTable>(Parent());
}

void LayoutNGTableSection::AddChild(LayoutObject* child,
                                    LayoutObject* before_child) {
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();

  if (!child->IsTableRow()) {
    LayoutObject* last = before_child;
    if (!last)
      last = LastChild();
    if (last && last->IsAnonymous() && last->IsTablePart() &&
        !last->IsBeforeOrAfterContent()) {
      if (before_child == last)
        before_child = last->SlowFirstChild();
      last->AddChild(child, before_child);
      return;
    }

    if (before_child && !before_child->IsAnonymous() &&
        before_child->Parent() == this) {
      LayoutObject* row = before_child->PreviousSibling();
      if (row && row->IsTableRow() && row->IsAnonymous()) {
        row->AddChild(child);
        return;
      }
    }

    // If before_child is inside an anonymous cell/row, insert into the cell or
    // into the anonymous row containing it, if there is one.
    LayoutObject* last_box = last;
    while (last_box && last_box->Parent()->IsAnonymous() &&
           !last_box->IsTableRow())
      last_box = last_box->Parent();
    if (last_box && last_box->IsAnonymous() &&
        !last_box->IsBeforeOrAfterContent()) {
      last_box->AddChild(child, before_child);
      return;
    }

    LayoutObject* row =
        LayoutObjectFactory::CreateAnonymousTableRowWithParent(*this);
    AddChild(row, before_child);
    row->AddChild(child);
    return;
  }
  if (before_child && before_child->Parent() != this)
    before_child = SplitAnonymousBoxesAroundChild(before_child);

  LayoutNGMixin<LayoutBlock>::AddChild(child, before_child);
}

void LayoutNGTableSection::RemoveChild(LayoutObject* child) {
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::RemoveChild(child);
}

void LayoutNGTableSection::StyleDidChange(StyleDifference diff,
                                          const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table()) {
    if ((old_style && !old_style->BorderVisuallyEqual(StyleRef())) ||
        (diff.TextDecorationOrColorChanged() &&
         StyleRef().HasBorderColorReferencingCurrentColor())) {
      table->GridBordersChanged();
    }
  }
  LayoutNGMixin<LayoutBlock>::StyleDidChange(diff, old_style);
}

LayoutBox* LayoutNGTableSection::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  return LayoutObjectFactory::CreateAnonymousTableSectionWithParent(*parent);
}

LayoutNGTableInterface* LayoutNGTableSection::TableInterface() const {
  return ToInterface<LayoutNGTableInterface>(Parent());
}

void LayoutNGTableSection::SetNeedsCellRecalc() {
  SetNeedsLayout(layout_invalidation_reason::kDomChanged);
}

LayoutNGTableRowInterface* LayoutNGTableSection::FirstRowInterface() const {
  return ToInterface<LayoutNGTableRowInterface>(FirstChild());
}

LayoutNGTableRowInterface* LayoutNGTableSection::LastRowInterface() const {
  return ToInterface<LayoutNGTableRowInterface>(LastChild());
}

const LayoutNGTableCellInterface* LayoutNGTableSection::PrimaryCellInterfaceAt(
    unsigned row,
    unsigned column) const {
  unsigned current_row = 0;
  for (LayoutObject* layout_row = FirstChild(); layout_row;
       layout_row = layout_row->NextSibling()) {
    DCHECK(layout_row->IsTableRow());
    if (current_row++ == row) {
      unsigned current_column = 0;
      for (LayoutObject* layout_cell = layout_row->SlowFirstChild();
           layout_cell; layout_cell = layout_cell->NextSibling()) {
        if (current_column++ == column) {
          return ToInterface<LayoutNGTableCellInterface>(layout_cell);
        }
      }
      return nullptr;
    }
  }
  return nullptr;
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::IsDataTable, verify
// behaviour is correct. Consider removing these methods.
unsigned LayoutNGTableSection::NumEffectiveColumns() const {
  return To<LayoutNGTable>(TableInterface()->ToLayoutObject())->ColumnCount();
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::IsDataTable/ColumnCount,
// verify behaviour is correct.
unsigned LayoutNGTableSection::NumCols(unsigned row) const {
  unsigned current_row = 0;
  for (LayoutObject* layout_row = FirstChild(); layout_row;
       layout_row = layout_row->NextSibling()) {
    if (current_row++ == row) {
      unsigned current_column = 0;
      for (LayoutObject* layout_cell = FirstChild(); layout_cell;
           layout_cell = layout_cell->NextSibling()) {
        ++current_column;
      }
      return current_column;
    }
  }
  return 0;
}

// TODO(crbug.com/1079133): Used by AXLayoutObject, verify behaviour is
// correct, and if caching is required.
unsigned LayoutNGTableSection::NumRows() const {
  unsigned num_rows = 0;
  for (LayoutObject* layout_row = FirstChild(); layout_row;
       layout_row = layout_row->NextSibling()) {
    // TODO(crbug.com/1079133) skip for abspos?
    ++num_rows;
  }
  return num_rows;
}

}  // namespace blink
