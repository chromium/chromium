// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

class LayoutNGTableCell;
class LayoutNGTable;

// NOTE:
// Legacy table row inherits from LayoutBox, not LayoutBlock.
// Every child of LayoutNGTableRow must be LayoutNGTableCell.
class CORE_EXPORT LayoutNGTableRow : public LayoutNGMixin<LayoutBlock>,
                                     public LayoutNGTableRowInterface {
 public:
  explicit LayoutNGTableRow(Element*);

  bool IsEmpty() const;

  LayoutNGTable* Table() const;

  // LayoutBlock methods start.

  void UpdateBlockLayout(bool relayout_children) override { NOTREACHED(); }

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGTableRow";
  }

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;

  void RemoveChild(LayoutObject*) override;

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  // Whether a row has opaque background depends on many factors, e.g. border
  // spacing, border collapsing, missing cells, etc.
  // For simplicity, just conservatively assume all table rows are not opaque.
  // Copied from Legacy's LayoutTableRow
  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override {
    NOT_DESTROYED();
    return false;
  }

  bool AllowsNonVisibleOverflow() const override {
    NOT_DESTROYED();
    return false;
  }

  void AddVisualOverflowFromBlockChildren() override;

  bool VisualRectRespectsVisibility() const final {
    NOT_DESTROYED();
    return false;
  }

  // LayoutBlock methods end.

  // LayoutNGTableRowInterface methods start.

  const LayoutObject* ToLayoutObject() const final {
    NOT_DESTROYED();
    return this;
  }

  const LayoutTableRow* ToLayoutTableRow() const final {
    NOTREACHED();
    return nullptr;
  }

  const LayoutNGTableRowInterface* ToLayoutNGTableRowInterface() const final {
    NOT_DESTROYED();
    return this;
  }

  LayoutNGTableInterface* TableInterface() const final {
    NOT_DESTROYED();
    return SectionInterface()->TableInterface();
  }

  unsigned RowIndex() const final;

  LayoutNGTableSectionInterface* SectionInterface() const final;

  LayoutNGTableRowInterface* PreviousRowInterface() const final;

  LayoutNGTableRowInterface* NextRowInterface() const final;

  LayoutNGTableCellInterface* FirstCellInterface() const final;

  LayoutNGTableCellInterface* LastCellInterface() const final;

  // LayoutNGTableRowInterface methods end.

 protected:
  LayoutNGTableCell* LastCell() const;

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectTableRow ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableRow> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableRow() && object.IsLayoutNGObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_H_
