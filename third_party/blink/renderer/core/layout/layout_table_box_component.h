// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_BOX_COMPONENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_BOX_COMPONENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/paint/paint_result.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

namespace blink {

// Common super class for LayoutTableCol, LayoutTableSection and LayoutTableRow.
// Also provides utility functions for all table parts.
class CORE_EXPORT LayoutTableBoxComponent : public LayoutBox {
 public:
  static void InvalidateCollapsedBordersOnStyleChange(
      const LayoutObject& table_part,
      LayoutTable&,
      const StyleDifference&,
      const ComputedStyle& old_style);

  static bool DoCellsHaveDirtyWidth(const LayoutObject& table_part,
                                    const LayoutTable&,
                                    const StyleDifference&,
                                    const ComputedStyle& old_style);

  class MutableForPainting : public LayoutObject::MutableForPainting {
   public:
    void UpdatePaintResult(PaintResult, const CullRect& paint_rect);

   private:
    friend class LayoutTableBoxComponent;
    MutableForPainting(const LayoutTableBoxComponent& box)
        : LayoutObject::MutableForPainting(box) {}
  };
  MutableForPainting GetMutableForPainting() const {
    CheckIsNotDestroyed();
    return MutableForPainting(*this);
  }

  // Should use TableStyle() instead of own style to determine cell order.
  const ComputedStyle& TableStyle() const {
    CheckIsNotDestroyed();
    return Table()->StyleRef();
  }

  BorderValue BorderStartInTableDirection() const {
    CheckIsNotDestroyed();
    return StyleRef().BorderStartUsing(TableStyle());
  }
  BorderValue BorderEndInTableDirection() const {
    CheckIsNotDestroyed();
    return StyleRef().BorderEndUsing(TableStyle());
  }
  BorderValue BorderBeforeInTableDirection() const {
    CheckIsNotDestroyed();
    return StyleRef().BorderBeforeUsing(TableStyle());
  }
  BorderValue BorderAfterInTableDirection() const {
    CheckIsNotDestroyed();
    return StyleRef().BorderAfterUsing(TableStyle());
  }

 protected:
  explicit LayoutTableBoxComponent(Element* element)
      : LayoutBox(element), last_paint_result_(kFullyPainted) {
    CheckIsNotDestroyed();
  }

  const LayoutObjectChildList* Children() const {
    CheckIsNotDestroyed();
    return &children_;
  }
  LayoutObjectChildList* Children() {
    CheckIsNotDestroyed();
    return &children_;
  }

  LayoutObject* FirstChild() const {
    CheckIsNotDestroyed();
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->FirstChild();
  }
  LayoutObject* LastChild() const {
    CheckIsNotDestroyed();
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->LastChild();
  }

 private:
  // Column, section and row's visibility has rules different from other
  // elements. For example, column's visibility:hidden doesn't apply; row's
  // visibility:hidden shouldn't hide row's background painted behind visible
  // cells, etc.
  bool VisualRectRespectsVisibility() const final {
    CheckIsNotDestroyed();
    return false;
  }

  // If you have a LayoutTableBoxComponent, use firstChild or lastChild instead.
  void SlowFirstChild() const = delete;
  void SlowLastChild() const = delete;

  LayoutObjectChildList* VirtualChildren() override {
    CheckIsNotDestroyed();
    return Children();
  }
  const LayoutObjectChildList* VirtualChildren() const override {
    CheckIsNotDestroyed();
    return Children();
  }

  virtual LayoutTable* Table() const = 0;

  LayoutObjectChildList children_;

  friend class MutableForPainting;
  PaintResult last_paint_result_;
  CullRect last_paint_rect_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_TABLE_BOX_COMPONENT_H_
