// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block.h"

namespace blink {

class LayoutNGTable;
class LayoutNGTableRow;

// NOTE:
// Every child of LayoutNGTableSection must be LayoutNGTableRow.
class CORE_EXPORT LayoutNGTableSection : public LayoutNGBlock {
 public:
  explicit LayoutNGTableSection(Element*);

  static LayoutNGTableSection* CreateAnonymousWithParent(const LayoutObject&);

  bool IsEmpty() const;

  LayoutNGTableRow* FirstRow() const;
  LayoutNGTableRow* LastRow() const;
  LayoutNGTable* Table() const;

  // LayoutBlock methods start.

  void UpdateBlockLayout(bool relayout_children) override {
    NOT_DESTROYED();
    NOTREACHED();
  }

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGTableSection";
  }

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;

  void RemoveChild(LayoutObject*) override;

  void WillBeRemovedFromTree() override;

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) override;

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  bool RespectsCSSOverflow() const override {
    NOT_DESTROYED();
    return false;
  }

  // Whether a section has opaque background depends on many factors, e.g.
  // border spacing, border collapsing, missing cells, etc. For simplicity,
  // just conservatively assume all table sections are not opaque.
  bool ForegroundIsKnownToBeOpaqueInRect(const PhysicalRect&,
                                         unsigned) const override {
    NOT_DESTROYED();
    return false;
  }

  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const override {
    NOT_DESTROYED();
    return false;
  }

  bool VisualRectRespectsVisibility() const final {
    NOT_DESTROYED();
    return false;
  }

  // LayoutBlock methods end.

  void SetNeedsCellRecalc();

  unsigned NumRows() const;

  unsigned NumCols(unsigned) const;

  unsigned NumEffectiveColumns() const;

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectTableSection ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableSection> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsTableSection();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_SECTION_H_
