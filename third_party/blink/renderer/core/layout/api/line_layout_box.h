// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BOX_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_box_model.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutBox;

class LineLayoutBox : public LineLayoutBoxModel {
 public:
  explicit LineLayoutBox(LayoutBox* layout_box)
      : LineLayoutBoxModel(layout_box) {}

  explicit LineLayoutBox(const LineLayoutItem& item)
      : LineLayoutBoxModel(item) {
    SECURITY_DCHECK(!item || item.IsBox());
  }

  explicit LineLayoutBox(std::nullptr_t) : LineLayoutBoxModel(nullptr) {}

  LineLayoutBox() = default;

  LayoutPoint Location() const { return ToBox()->Location(); }
  PhysicalOffset PhysicalLocation() const {
    return ToBox()->PhysicalLocation();
  }

  LayoutSize Size() const { return ToBox()->Size(); }

  void SetLogicalHeight(LayoutUnit size) { ToBox()->SetLogicalHeight(size); }

  LayoutUnit LogicalHeight() const { return ToBox()->LogicalHeight(); }

  LayoutUnit LogicalTop() const { return ToBox()->LogicalTop(); }

  LayoutUnit LogicalBottom() const { return ToBox()->LogicalBottom(); }

  LayoutUnit FlipForWritingMode(LayoutUnit unit) const {
    return ToBox()->FlipForWritingMode(unit);
  }

  void FlipForWritingMode(LayoutRect& rect) const {
    ToBox()->DeprecatedFlipForWritingMode(rect);
  }

  LayoutPoint FlipForWritingMode(const LayoutPoint& point) const {
    return ToBox()->DeprecatedFlipForWritingMode(point);
  }

  void MoveWithEdgeOfInlineContainerIfNecessary(bool is_horizontal) {
    ToBox()->MoveWithEdgeOfInlineContainerIfNecessary(is_horizontal);
  }

  void Move(const LayoutUnit& width, const LayoutUnit& height) {
    ToBox()->Move(width, height);
  }

  bool HasLayoutOverflow() const { return ToBox()->HasLayoutOverflow(); }
  bool HasVisualOverflow() const { return ToBox()->HasVisualOverflow(); }
  LayoutRect LogicalVisualOverflowRectForPropagation() const {
    return ToBox()->LogicalVisualOverflowRectForPropagation();
  }
  LayoutRect LogicalLayoutOverflowRectForPropagation() const {
    return ToBox()->LogicalLayoutOverflowRectForPropagation(nullptr);
  }

  void SetLocation(const LayoutPoint& location) {
    return ToBox()->SetLocation(location);
  }

  void SetSize(const LayoutSize& size) { return ToBox()->SetSize(size); }

  LayoutSize ScrolledContentOffset() const {
    return ToBox()->ScrolledContentOffset();
  }

  InlineBox* CreateInlineBox() { return ToBox()->CreateInlineBox(); }

  InlineBox* InlineBoxWrapper() const { return ToBox()->InlineBoxWrapper(); }

  void SetInlineBoxWrapper(InlineBox* box) {
    return ToBox()->SetInlineBoxWrapper(box);
  }

#if DCHECK_IS_ON()

  void ShowLineTreeAndMark(const InlineBox* marked_box1,
                           const char* marked_label1) const {
    if (auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(GetLayoutObject()))
      layout_block_flow->ShowLineTreeAndMark(marked_box1, marked_label1);
  }

#endif

 private:
  LayoutBox* ToBox() { return ToLayoutBox(GetLayoutObject()); }

  const LayoutBox* ToBox() const { return ToLayoutBox(GetLayoutObject()); }
};

inline LineLayoutBox LineLayoutItem::ContainingBlock() const {
  return LineLayoutBox(GetLayoutObject()->ContainingBlock());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BOX_H_
