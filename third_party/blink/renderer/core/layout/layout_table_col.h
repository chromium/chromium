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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_COL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_COL_H_

#include "third_party/blink/renderer/core/layout/layout_table_box_component.h"

namespace blink {

class LayoutTable;

// LayoutTableCol is used to represent table column or column groups
// (display: table-column and display: table-column-group).
//
// The reason to use the same LayoutObject is that both objects behave in a very
// similar way. The main difference between the 2 is that table-column-group
// allows table-column children, when table-column don't.
// Note that this matches how <col> and <colgroup> map to the same class:
// HTMLTableColElement.
//
// In HTML and CSS, table columns and colgroups don't own the cells, they are
// descendants of the rows.
// As such table columns and colgroups have a very limited scope in the table:
// - column / cell sizing (the 'width' property)
// - background painting (the 'background' property).
// - border collapse resolution
//   (http://www.w3.org/TR/CSS21/tables.html#border-conflict-resolution)
//
// See http://www.w3.org/TR/CSS21/tables.html#columns for the standard.
// Note that we don't implement the "visibility: collapse" inheritance to the
// cells.
//
// Because table columns and column groups are placeholder elements (see
// previous paragraph), they are never laid out and layout() should not be
// called on them.
class LayoutTableCol final : public LayoutTableBoxComponent {
 public:
  explicit LayoutTableCol(Element*);

  void ClearIntrinsicLogicalWidthsDirtyBits();

  // The 'span' attribute in HTML.
  // For CSS table columns or colgroups, this is always 1.
  unsigned Span() const { return span_; }

  bool IsTableColumnGroupWithColumnChildren() { return FirstChild(); }
  bool IsTableColumn() const {
    return StyleRef().Display() == EDisplay::kTableColumn;
  }
  bool IsTableColumnGroup() const {
    return StyleRef().Display() == EDisplay::kTableColumnGroup;
  }

  LayoutTableCol* EnclosingColumnGroup() const;

  // Returns the next column or column-group.
  LayoutTableCol* NextColumn() const;

  const char* GetName() const override { return "LayoutTableCol"; }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutTableCol || LayoutBox::IsOfType(type);
  }
  void UpdateFromElement() override;

  MinMaxSizes PreferredLogicalWidths() const override {
    NOTREACHED();
    return MinMaxSizes();
  }
  MinMaxSizes ComputeIntrinsicLogicalWidths() const final {
    NOTREACHED();
    return MinMaxSizes();
  }

  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;
  bool CanHaveChildren() const override;
  PaintLayerType LayerTypeRequired() const override { return kNoPaintLayer; }

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  LayoutTable* Table() const final;

  unsigned span_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTableCol, IsLayoutTableCol());

}  // namespace blink

#endif
