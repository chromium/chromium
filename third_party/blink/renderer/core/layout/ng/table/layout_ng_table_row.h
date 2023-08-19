// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_H_

#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"

namespace blink {

class LayoutNGTableCell;
class LayoutNGTableSection;
class LayoutNGTable;

// Every child of LayoutNGTableRow must be LayoutNGTableCell.
class CORE_EXPORT LayoutNGTableRow : public LayoutNGBlock {
 public:
  explicit LayoutNGTableRow(Element*);

  static LayoutNGTableRow* CreateAnonymousWithParent(const LayoutObject&);

  bool IsEmpty() const;

  LayoutNGTableCell* FirstCell() const;
  LayoutNGTableCell* LastCell() const;
  LayoutNGTableRow* NextRow() const;
  LayoutNGTableRow* PreviousRow() const;
  LayoutNGTableSection* Section() const;
  LayoutNGTable* Table() const;

  // LayoutBlock methods start.

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGTableRow";
  }

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;

  void RemoveChild(LayoutObject*) override;

  void WillBeRemovedFromTree() override;

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  LayoutBlock* StickyContainer() const override;

  // Whether a row has opaque background depends on many factors, e.g. border
  // spacing, border collapsing, missing cells, etc.
  // For simplicity, just conservatively assume all table rows are not opaque.
  bool ForegroundIsKnownToBeOpaqueInRect(const PhysicalRect&,
                                         unsigned) const override {
    NOT_DESTROYED();
    return false;
  }

  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override {
    NOT_DESTROYED();
    return false;
  }

  bool RespectsCSSOverflow() const override {
    NOT_DESTROYED();
    return false;
  }

#if DCHECK_IS_ON()
  void AddVisualOverflowFromBlockChildren() override;
#endif

  bool VisualRectRespectsVisibility() const final {
    NOT_DESTROYED();
    return false;
  }

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  // LayoutBlock methods end.

  unsigned RowIndex() const;

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectTableRow ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }

  // Table section paints background specially.
  bool ComputeCanCompositeBackgroundAttachmentFixed() const override {
    NOT_DESTROYED();
    return false;
  }
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableRow> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableRow();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_ROW_H_
