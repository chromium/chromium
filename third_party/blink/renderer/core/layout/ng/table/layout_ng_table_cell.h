// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/table_constants.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell_interface.h"

namespace blink {

class LayoutNGTable;

class CORE_EXPORT LayoutNGTableCell
    : public LayoutNGBlockFlowMixin<LayoutBlockFlow>,
      public LayoutNGTableCellInterface {
 public:
  explicit LayoutNGTableCell(Element*);

  // NOTE: Rowspan might overflow section boundaries.
  unsigned ComputedRowSpan() const {
    NOT_DESTROYED();
    if (!has_rowspan_)
      return 1;
    unsigned rowspan = ParseRowSpanFromDOM();
    if (rowspan == 0)  // rowspan == 0 means all rows.
      rowspan = kMaxRowSpan;
    return rowspan;
  }

  const NGBoxStrut& IntrinsicLogicalWidthsBorderSizes() const {
    NOT_DESTROYED();
    return intrinsical_logical_widths_border_sizes_;
  }

  void SetIntrinsicLogicalWidthsBorderSizes(const NGBoxStrut& border_sizes) {
    NOT_DESTROYED();
    intrinsical_logical_widths_border_sizes_ = border_sizes;
  }

  // This method is called after a new *measure* layout-result is set.
  // Tables are special in that the table accesses the table-cells directly
  // for computing the size of the table-grid (bypassing the section/row).
  // Due to this when we set a new measure layout-result, we also invalidate
  // the layout-result cache of the associated section/row (as the cell
  // fragment would be in the incorrect state).
  void InvalidateLayoutResultCacheAfterMeasure() const;

  LayoutUnit BorderTop() const override;

  LayoutUnit BorderBottom() const override;

  LayoutUnit BorderLeft() const override;

  LayoutUnit BorderRight() const override;

  LayoutRectOutsets BorderBoxOutsets() const override;

  LayoutNGTable* Table() const;

  // LayoutBlockFlow methods start.

  void UpdateBlockLayout(bool relayout_children) override;

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) final;

  void WillBeRemovedFromTree() override;

  const char* GetName() const final {
    NOT_DESTROYED();
    return "LayoutNGTableCell";
  }

  bool CreatesNewFormattingContext() const final {
    NOT_DESTROYED();
    return true;
  }

  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override;

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  LayoutBlock* StickyContainer() const override;

  void InvalidatePaint(const PaintInvalidatorContext&) const override;

  // LayoutBlockFlow methods end.

  // LayoutNGTableCellInterface methods start.

  const LayoutNGTableCellInterface* ToLayoutNGTableCellInterface() const final {
    NOT_DESTROYED();
    return this;
  }
  const LayoutObject* ToLayoutObject() const final {
    NOT_DESTROYED();
    return this;
  }

  LayoutObject* ToMutableLayoutObject() final {
    NOT_DESTROYED();
    return this;
  }

  LayoutNGTableInterface* TableInterface() const final;

  void ColSpanOrRowSpanChanged() final;

  unsigned RowIndex() const final;

  unsigned ResolvedRowSpan() const final;

  unsigned AbsoluteColumnIndex() const final;

  // Guaranteed to be between kMinColSpan and kMaxColSpan.
  unsigned ColSpan() const final;

  LayoutNGTableCellInterface* NextCellInterface() const final;

  LayoutNGTableCellInterface* PreviousCellInterface() const final;

  LayoutNGTableRowInterface* RowInterface() const final;

  LayoutNGTableSectionInterface* SectionInterface() const final;

  // LayoutNGTableCellInterface methods end.

 protected:
  bool IsOfType(LayoutObjectType type) const final {
    NOT_DESTROYED();
    return type == kLayoutObjectTableCell ||
           LayoutNGBlockFlowMixin<LayoutBlockFlow>::IsOfType(type);
  }

 private:
  void UpdateColAndRowSpanFlags();

  unsigned ParseRowSpanFromDOM() const;

  unsigned ParseColSpanFromDOM() const;
  // Use ComputedRowSpan instead
  unsigned ParsedRowSpan() const {
    NOT_DESTROYED();
    if (!has_rowspan_)
      return 1;
    return ParseRowSpanFromDOM();
  }

  // Cached cell border. Used to invalidate calculation of
  // intrinsic logical width.
  NGBoxStrut intrinsical_logical_widths_border_sizes_;

  unsigned has_col_span_ : 1;
  unsigned has_rowspan_ : 1;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableCell> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableCell() && !object.IsTableCellLegacy();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_CELL_H_
