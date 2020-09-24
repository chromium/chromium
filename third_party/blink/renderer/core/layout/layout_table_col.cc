/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/core/layout/layout_table_col.h"

#include "third_party/blink/renderer/core/html/html_table_col_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"

namespace blink {

LayoutTableCol::LayoutTableCol(Element* element)
    : LayoutTableBoxComponent(element), span_(1) {
  // init LayoutObject attributes
  SetInline(true);  // our object is not Inline
  UpdateFromElement();
}

void LayoutTableCol::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  DCHECK(StyleRef().Display() == EDisplay::kTableColumn ||
         StyleRef().Display() == EDisplay::kTableColumnGroup);

  LayoutTableBoxComponent::StyleDidChange(diff, old_style);

  if (!old_style)
    return;

  LayoutTable* table = Table();
  if (!table)
    return;

  LayoutTableBoxComponent::InvalidateCollapsedBordersOnStyleChange(
      *this, *table, diff, *old_style);

  if ((old_style->LogicalWidth() != StyleRef().LogicalWidth()) ||
      LayoutTableBoxComponent::DoCellsHaveDirtyWidth(*this, *table, diff,
                                                     *old_style)) {
    // TODO(dgrogan): Optimization opportunities:
    // (1) Only mark cells which are affected by this col, not every cell in the
    //     table.
    // (2) If only the col width changes and its border width doesn't, do the
    //     cells need to be marked as needing layout or just given dirty
    //     widths?
    table->MarkAllCellsWidthsDirtyAndOrNeedsLayout(
        LayoutTable::kMarkDirtyAndNeedsLayout);
  }
}

void LayoutTableCol::UpdateFromElement() {
  unsigned old_span = span_;

  if (auto* tc = DynamicTo<HTMLTableColElement>(GetNode())) {
    span_ = tc->span();
  } else {
    span_ = 1;
  }
  if (span_ != old_span && Style() && Parent()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kAttributeChanged);
  }
}

void LayoutTableCol::InsertedIntoTree() {
  LayoutTableBoxComponent::InsertedIntoTree();
  Table()->AddColumn(this);
}

void LayoutTableCol::WillBeRemovedFromTree() {
  LayoutTableBoxComponent::WillBeRemovedFromTree();
  Table()->RemoveColumn(this);
}

bool LayoutTableCol::IsChildAllowed(LayoutObject* child,
                                    const ComputedStyle& style) const {
  // We cannot use isTableColumn here as style() may return 0.
  return child->IsLayoutTableCol() && style.Display() == EDisplay::kTableColumn;
}

bool LayoutTableCol::CanHaveChildren() const {
  // Cols cannot have children. This is actually necessary to fix a bug
  // with libraries.uc.edu, which makes a <p> be a table-column.
  return IsTableColumnGroup();
}

void LayoutTableCol::ClearIntrinsicLogicalWidthsDirtyBits() {
  ClearIntrinsicLogicalWidthsDirty();

  for (LayoutObject* child = FirstChild(); child; child = child->NextSibling())
    child->ClearIntrinsicLogicalWidthsDirty();
}

LayoutTable* LayoutTableCol::Table() const {
  LayoutObject* table = Parent();
  if (table && !table->IsTable())
    table = table->Parent();
  return table && table->IsTable() ? To<LayoutTable>(table) : nullptr;
}

LayoutTableCol* LayoutTableCol::EnclosingColumnGroup() const {
  if (!Parent()->IsLayoutTableCol())
    return nullptr;

  LayoutTableCol* parent_column_group = ToLayoutTableCol(Parent());
  DCHECK(parent_column_group->IsTableColumnGroup());
  DCHECK(IsTableColumn());
  return parent_column_group;
}

LayoutTableCol* LayoutTableCol::NextColumn() const {
  // If |this| is a column-group, the next column is the colgroup's first child
  // column.
  if (LayoutObject* first_child = FirstChild())
    return ToLayoutTableCol(first_child);

  // Otherwise it's the next column along.
  LayoutObject* next = NextSibling();

  // Failing that, the child is the last column in a column-group, so the next
  // column is the next column/column-group after its column-group.
  if (!next && Parent()->IsLayoutTableCol())
    next = Parent()->NextSibling();

  for (; next && !next->IsLayoutTableCol(); next = next->NextSibling()) {
  }

  return ToLayoutTableCol(next);
}

}  // namespace blink
