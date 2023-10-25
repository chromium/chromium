// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/table_borders.h"

namespace blink {

LayoutTableSection::LayoutTableSection(Element* element)
    : LayoutBlock(element) {}

LayoutTableSection* LayoutTableSection::CreateAnonymousWithParent(
    const LayoutObject& parent) {
  const ComputedStyle* new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(), EDisplay::kTableRowGroup);
  auto* new_section = MakeGarbageCollected<LayoutTableSection>(nullptr);
  new_section->SetDocumentForAnonymous(&parent.GetDocument());
  new_section->SetStyle(new_style);
  return new_section;
}

bool LayoutTableSection::IsEmpty() const {
  NOT_DESTROYED();
  return !FirstChild();
}

LayoutTableRow* LayoutTableSection::FirstRow() const {
  NOT_DESTROYED();
  return To<LayoutTableRow>(FirstChild());
}

LayoutTableRow* LayoutTableSection::LastRow() const {
  NOT_DESTROYED();
  return To<LayoutTableRow>(LastChild());
}

LayoutTable* LayoutTableSection::Table() const {
  NOT_DESTROYED();
  return To<LayoutTable>(Parent());
}

void LayoutTableSection::AddChild(LayoutObject* child,
                                  LayoutObject* before_child) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->TableGridStructureChanged();
  }

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

    auto* row = LayoutTableRow::CreateAnonymousWithParent(*this);
    AddChild(row, before_child);
    row->AddChild(child);
    return;
  }
  if (before_child && before_child->Parent() != this)
    before_child = SplitAnonymousBoxesAroundChild(before_child);

  LayoutBlock::AddChild(child, before_child);
}

void LayoutTableSection::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->TableGridStructureChanged();
  }
  LayoutBlock::RemoveChild(child);
}

void LayoutTableSection::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->TableGridStructureChanged();
  }
  LayoutBlock::WillBeRemovedFromTree();
}

void LayoutTableSection::StyleDidChange(StyleDifference diff,
                                        const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    if ((old_style && !old_style->BorderVisuallyEqual(StyleRef())) ||
        (old_style && old_style->GetWritingDirection() !=
                          StyleRef().GetWritingDirection())) {
      table->GridBordersChanged();
    }
  }
  LayoutBlock::StyleDidChange(diff, old_style);
}

LayoutBox* LayoutTableSection::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return CreateAnonymousWithParent(*parent);
}

// TODO(crbug.com/1079133): Used by AXLayoutObject, verify behaviour is
// correct, and if caching is required.
unsigned LayoutTableSection::NumRows() const {
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
