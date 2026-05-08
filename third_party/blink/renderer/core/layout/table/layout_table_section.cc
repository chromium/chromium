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

void LayoutTableSection::AddChildBeforeDescendant(
    LayoutObject* new_child,
    LayoutObject* before_descendant) {
  NOT_DESTROYED();
  DCHECK_NE(before_descendant->Parent(), this);

  if (new_child->IsTableRow()) {
    LayoutObject* before_child =
        SplitAnonymousBoxesAroundChild(before_descendant);
    DCHECK_EQ(before_child->Parent(), this);
    AddChild(new_child, before_child);
    return;
  }

  LayoutObject* before_descendant_container = before_descendant->Parent();
  while (before_descendant_container->Parent() != this) {
    before_descendant_container = before_descendant_container->Parent();
  }
  CHECK(before_descendant_container->IsAnonymous());
  CHECK(before_descendant_container->IsTableRow());

  // Insert the child into the anonymous table-row instead of here.
  before_descendant_container->AddChild(new_child, before_descendant);
}

void LayoutTableSection::AddChild(LayoutObject* new_child,
                                  LayoutObject* before_child) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->TableGridStructureChanged();
  }

  if (before_child && before_child->Parent() != this) {
    AddChildBeforeDescendant(new_child, before_child);
    return;
  }

  if (!new_child->IsTableRow()) {
    LayoutObject* after_child =
        before_child ? before_child->PreviousSibling() : LastChild();

    if (after_child && after_child->IsAnonymous()) {
      after_child->AddChild(new_child);
      return;
    }

    // No suitable existing anonymous table-row - create a new one.
    LayoutTableRow* row = LayoutTableRow::CreateAnonymousWithParent(*this);
    LayoutBox::AddChild(row, before_child);
    row->AddChild(new_child);
    return;
  }

  LayoutBox::AddChild(new_child, before_child);
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

void LayoutTableSection::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    if ((old_style && !old_style->BorderVisuallyEqual(StyleRef())) ||
        (old_style && old_style->GetWritingDirection() !=
                          StyleRef().GetWritingDirection())) {
      table->GridBordersChanged();
    }
  }
  LayoutBlock::StyleDidChange(diff, old_style, style_change_context);
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
