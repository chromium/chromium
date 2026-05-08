// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/table/table_borders.h"

namespace blink {

LayoutTableRow::LayoutTableRow(Element* element) : LayoutBlock(element) {}

LayoutTableRow* LayoutTableRow::CreateAnonymousWithParent(
    const LayoutObject& parent) {
  const ComputedStyle* new_style =
      parent.GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent.StyleRef(), EDisplay::kTableRow);
  auto* new_row = MakeGarbageCollected<LayoutTableRow>(nullptr);
  new_row->SetDocumentForAnonymous(&parent.GetDocument());
  new_row->SetStyle(new_style);
  return new_row;
}

LayoutTableCell* LayoutTableRow::FirstCell() const {
  NOT_DESTROYED();
  return To<LayoutTableCell>(FirstChild());
}

LayoutTableCell* LayoutTableRow::LastCell() const {
  NOT_DESTROYED();
  return To<LayoutTableCell>(LastChild());
}

LayoutTableRow* LayoutTableRow::NextRow() const {
  NOT_DESTROYED();
  return To<LayoutTableRow>(NextSibling());
}

LayoutTableRow* LayoutTableRow::PreviousRow() const {
  NOT_DESTROYED();
  return To<LayoutTableRow>(PreviousSibling());
}

LayoutTableSection* LayoutTableRow::Section() const {
  NOT_DESTROYED();
  return To<LayoutTableSection>(Parent());
}

LayoutTable* LayoutTableRow::Table() const {
  NOT_DESTROYED();
  if (LayoutObject* section = Parent()) {
    if (LayoutObject* table = section->Parent())
      return To<LayoutTable>(table);
  }
  return nullptr;
}

void LayoutTableRow::AddChildBeforeDescendant(LayoutObject* new_child,
                                              LayoutObject* before_descendant) {
  NOT_DESTROYED();
  DCHECK_NE(before_descendant->Parent(), this);

  if (new_child->IsTableCell()) {
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
  CHECK(before_descendant_container->IsTableCell());

  // Insert the child into the anonymous table-cell instead of here.
  before_descendant_container->AddChild(new_child, before_descendant);
}

void LayoutTableRow::AddChild(LayoutObject* new_child,
                              LayoutObject* before_child) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->TableGridStructureChanged();
  }

  if (before_child && before_child->Parent() != this) {
    AddChildBeforeDescendant(new_child, before_child);
    return;
  }

  if (!new_child->IsTableCell()) {
    LayoutObject* after_child =
        before_child ? before_child->PreviousSibling() : LastChild();

    if (after_child && after_child->IsAnonymous()) {
      after_child->AddChild(new_child);
      return;
    }

    // No suitable existing anonymous table-cell - create a new one.
    LayoutTableCell* cell = LayoutTableCell::CreateAnonymousWithParent(*this);
    LayoutBox::AddChild(cell, before_child);
    cell->AddChild(new_child);
    return;
  }

  LayoutBox::AddChild(new_child, before_child);
}

void LayoutTableRow::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->TableGridStructureChanged();
  }
  // Invalidate background in case this doesn't need layout which would
  // trigger the invalidation, e.g. when the last child is removed.
  if (StyleRef().HasBackground()) {
    SetBackgroundNeedsFullPaintInvalidation();
  }

  LayoutBlock::RemoveChild(child);
}

void LayoutTableRow::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  if (LayoutTable* table = Table()) {
    table->TableGridStructureChanged();
  }
  LayoutBlock::WillBeRemovedFromTree();
}

void LayoutTableRow::StyleDidChange(
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

LayoutBox* LayoutTableRow::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return CreateAnonymousWithParent(*parent);
}

LayoutBlock* LayoutTableRow::StickyContainer() const {
  NOT_DESTROYED();
  return Table();
}

PositionWithAffinity LayoutTableRow::PositionForPoint(
    const PhysicalOffset& offset) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  // LayoutBlock::PositionForPoint is wrong for rows.
  return LayoutBox::PositionForPoint(offset);
}

unsigned LayoutTableRow::RowIndex() const {
  NOT_DESTROYED();
  unsigned index = 0;
  for (LayoutObject* child = Parent()->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child == this)
      return index;
    ++index;
  }
  NOTREACHED();
}

}  // namespace blink
