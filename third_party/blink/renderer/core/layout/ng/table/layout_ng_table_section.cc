// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"

namespace blink {

LayoutNGTableSection::LayoutNGTableSection(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

LayoutNGTableSection* LayoutNGTableSection::CreateAnonymousWithParent(
    const LayoutObject& parent) {
  const ComputedStyle* new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(), EDisplay::kTableRowGroup);
  auto* new_section = MakeGarbageCollected<LayoutNGTableSection>(nullptr);
  new_section->SetDocumentForAnonymous(&parent.GetDocument());
  new_section->SetStyle(new_style);
  return new_section;
}

bool LayoutNGTableSection::IsEmpty() const {
  NOT_DESTROYED();
  return !FirstChild();
}

LayoutNGTableRow* LayoutNGTableSection::FirstRow() const {
  NOT_DESTROYED();
  return To<LayoutNGTableRow>(FirstChild());
}

LayoutNGTableRow* LayoutNGTableSection::LastRow() const {
  NOT_DESTROYED();
  return To<LayoutNGTableRow>(LastChild());
}

LayoutNGTable* LayoutNGTableSection::Table() const {
  NOT_DESTROYED();
  return To<LayoutNGTable>(Parent());
}

void LayoutNGTableSection::AddChild(LayoutObject* child,
                                    LayoutObject* before_child) {
  NOT_DESTROYED();
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

    auto* row = LayoutNGTableRow::CreateAnonymousWithParent(*this);
    AddChild(row, before_child);
    row->AddChild(child);
    return;
  }
  if (before_child && before_child->Parent() != this)
    before_child = SplitAnonymousBoxesAroundChild(before_child);

  LayoutNGMixin<LayoutBlock>::AddChild(child, before_child);
}

void LayoutNGTableSection::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::RemoveChild(child);
}

void LayoutNGTableSection::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  if (LayoutNGTable* table = Table())
    table->TableGridStructureChanged();
  LayoutNGMixin<LayoutBlock>::WillBeRemovedFromTree();
}

void LayoutNGTableSection::StyleDidChange(StyleDifference diff,
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

LayoutBox* LayoutNGTableSection::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return CreateAnonymousWithParent(*parent);
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::IsDataTable, verify
// behaviour is correct. Consider removing these methods.
unsigned LayoutNGTableSection::NumEffectiveColumns() const {
  NOT_DESTROYED();
  const LayoutNGTable* table = Table();
  DCHECK(table);
  wtf_size_t column_count = table->ColumnCount();
  if (column_count == 0)
    return 0;
  return table->AbsoluteColumnToEffectiveColumn(column_count - 1) + 1;
}

// TODO(crbug.com/1079133): Used by AXLayoutObject::IsDataTable/ColumnCount,
// verify behaviour is correct.
unsigned LayoutNGTableSection::NumCols(unsigned row) const {
  NOT_DESTROYED();
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
  NOT_DESTROYED();
  unsigned num_rows = 0;
  for (LayoutObject* layout_row = FirstChild(); layout_row;
       layout_row = layout_row->NextSibling()) {
    // TODO(crbug.com/1079133) skip for abspos?
    ++num_rows;
  }
  return num_rows;
}

}  // namespace blink
