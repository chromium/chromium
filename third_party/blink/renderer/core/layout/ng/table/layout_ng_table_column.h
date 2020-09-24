// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_COLUMN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_COLUMN_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

class LayoutNGTable;

// Represents <col> and <colgroup> elements.
class CORE_EXPORT LayoutNGTableColumn : public LayoutBox {
 public:
  explicit LayoutNGTableColumn(Element*);

  LayoutNGTable* Table() const;

  bool IsColumn() const {
    return StyleRef().Display() == EDisplay::kTableColumn;
  }

  bool IsColumnGroup() const {
    return StyleRef().Display() == EDisplay::kTableColumnGroup;
  }

  unsigned Span() const { return span_; }

  // Clears needs-layout for child columns too.
  void ClearNeedsLayoutForChildren() const;

  // LayoutObject methods start.

  const char* GetName() const override {
    if (IsColumn())
      return "LayoutNGTableCol";
    else
      return "LayoutNGTableColGroup";
  }

  bool IsLayoutNGObject() const final { return true; }

  void StyleDidChange(StyleDifference diff,
                      const ComputedStyle* old_style) final;

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) final;

 protected:
  // Required by LayoutBox, but not used.
  MinMaxSizes ComputeIntrinsicLogicalWidths() const override {
    NOTIMPLEMENTED();
    return MinMaxSizes();
  }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutTableCol || LayoutBox::IsOfType(type);
  }

 private:
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  bool CanHaveChildren() const override;

  void UpdateFromElement() override;

  PaintLayerType LayerTypeRequired() const override { return kNoPaintLayer; }

  LayoutObjectChildList* VirtualChildren() override { return &children_; }

  const LayoutObjectChildList* VirtualChildren() const override {
    return &children_;
  }

  // LayoutObject methods end

 private:
  unsigned span_ = 1;
  LayoutObjectChildList children_;
};

// wtf/casting.h helper.
template <>
struct DowncastTraits<LayoutNGTableColumn> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutTableCol() && object.IsLayoutNGObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_LAYOUT_NG_TABLE_COLUMN_H_
