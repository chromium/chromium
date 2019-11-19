/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009, 2013 Apple Inc.
 *               All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_ROW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_ROW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_row_interface.h"

namespace blink {

// There is a window of opportunity to read |m_rowIndex| before it is set when
// inserting the LayoutTableRow or during LayoutTableSection::recalcCells.
// This value is used to detect that case.
static const unsigned kUnsetRowIndex = 0x7FFFFFFF;
static const unsigned kMaxRowIndex = 0x7FFFFFFE;  // 2,147,483,646

// LayoutTableRow is used to represent a table row (display: table-row).
//
// LayoutTableRow is a simple object. The reason is that most operations
// have to be coordinated at the LayoutTableSection level and thus are
// handled in LayoutTableSection (see e.g. layoutRows).
//
// The table model prevents any layout overflow on rows (but allow visual
// overflow). For row height, it is defined as "the maximum of the row's
// computed 'height', the computed 'height' of each cell in the row, and
// the minimum height (MIN) required by  the cells" (CSS 2.1 - section 17.5.3).
// Note that this means that rows and cells differ from other LayoutObject as
// they will ignore 'height' in some situation (when other LayoutObject just
// allow some layout overflow to occur).
//
// LayoutTableRow doesn't establish a containing block for the underlying
// LayoutTableCells. That's why it inherits from LayoutTableBoxComponent and not
// LayoutBlock.
// One oddity is that LayoutTableRow doesn't establish a new coordinate system
// for its children. LayoutTableCells are positioned with respect to the
// enclosing LayoutTableSection (this object's parent()). This particularity is
// why functions accumulating offset while walking the tree have to special case
// LayoutTableRow (see e.g. PaintInvalidatorContext or
// LayoutBox::PositionFromPoint()).
//
// LayoutTableRow is also positioned with respect to the enclosing
// LayoutTableSection. See LayoutTableSection::layoutRows() for the placement
// logic.
class CORE_EXPORT LayoutTableRow final : public LayoutTableBoxComponent,
                                         public LayoutNGTableRowInterface {
 public:
  explicit LayoutTableRow(Element*);

  LayoutTableCell* FirstCell() const;
  LayoutTableCell* LastCell() const;

  LayoutTableRow* PreviousRow() const;
  LayoutTableRow* NextRow() const;

  LayoutTableSection* Section() const {
    return To<LayoutTableSection>(Parent());
  }
  LayoutTable* Table() const final {
    return To<LayoutTable>(Parent()->Parent());
  }

  static LayoutTableRow* CreateAnonymous(Document*);
  static LayoutTableRow* CreateAnonymousWithParent(const LayoutObject*);
  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override {
    return CreateAnonymousWithParent(parent);
  }

  void SetRowIndex(unsigned row_index) {
    CHECK_LE(row_index, kMaxRowIndex);
    row_index_ = row_index;
  }

  bool RowIndexWasSet() const { return row_index_ != kUnsetRowIndex; }
  unsigned RowIndex() const final {
    DCHECK(RowIndexWasSet());
    DCHECK(
        !Section() ||
        !Section()
             ->NeedsCellRecalc());  // index may be bogus if cells need recalc.
    return row_index_;
  }

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) override;

  PaginationBreakability GetPaginationBreakability() const final;

  void ComputeLayoutOverflow();

  void RecalcVisualOverflow() override;

  const char* GetName() const override { return "LayoutTableRow"; }

  // Whether a row has opaque background depends on many factors, e.g. border
  // spacing, border collapsing, missing cells, etc.
  // For simplicity, just conservatively assume all table rows are not opaque.
  bool ForegroundIsKnownToBeOpaqueInRect(const PhysicalRect&,
                                         unsigned) const override {
    return false;
  }
  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override {
    return false;
  }
  bool PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const override;

  // LayoutNGTableRowInterface methods start.

  const LayoutNGTableRowInterface* ToLayoutNGTableRowInterface() const final {
    return this;
  }
  const LayoutObject* ToLayoutObject() const final { return this; }
  const LayoutTableRow* ToLayoutTableRow() const final { return this; }
  LayoutNGTableInterface* TableInterface() const final { return Table(); }
  LayoutNGTableSectionInterface* SectionInterface() const final {
    return Section();
  }
  LayoutNGTableRowInterface* NextRowInterface() const final {
    return NextRow();
  }
  LayoutNGTableRowInterface* PreviousRowInterface() const final {
    return PreviousRow();
  }
  LayoutNGTableCellInterface* FirstCellInterface() const final;
  LayoutNGTableCellInterface* LastCellInterface() const final;

  // LayoutNGTableRowInterface methods end.

 private:
  void ComputeVisualOverflow();
  void AddLayoutOverflowFromCell(const LayoutTableCell*);
  void AddVisualOverflowFromCell(const LayoutTableCell*);

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTableRow ||
           LayoutTableBoxComponent::IsOfType(type);
  }

  void WillBeRemovedFromTree() override;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;
  void UpdateLayout() override;

  PaintLayerType LayerTypeRequired() const override {
    if (HasTransformRelatedProperty() || HasHiddenBackface() ||
        CreatesGroup() || StyleRef().ShouldCompositeForCurrentAnimations() ||
        IsStickyPositioned())
      return kNormalPaintLayer;

    if (HasOverflowClip())
      return kOverflowClipPaintLayer;

    return kNoPaintLayer;
  }

  void Paint(const PaintInfo&) const override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void NextSibling() const = delete;
  void PreviousSibling() const = delete;

  // This field should never be read directly. It should be read through
  // rowIndex() above instead. This is to ensure that we never read this
  // value before it is set.
  unsigned row_index_ : 31;
};

template <>
struct DowncastTraits<LayoutTableRow> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableRow();
  }
};

inline LayoutTableRow* LayoutTableRow::PreviousRow() const {
  return To<LayoutTableRow>(LayoutObject::PreviousSibling());
}

inline LayoutTableRow* LayoutTableRow::NextRow() const {
  return To<LayoutTableRow>(LayoutObject::NextSibling());
}

inline LayoutTableRow* LayoutTableSection::FirstRow() const {
  return To<LayoutTableRow>(FirstChild());
}

inline LayoutTableRow* LayoutTableSection::LastRow() const {
  return To<LayoutTableRow>(LastChild());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_ROW_H_
