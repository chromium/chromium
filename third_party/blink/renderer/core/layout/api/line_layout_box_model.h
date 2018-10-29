// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BOX_MODEL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BOX_MODEL_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutBoxModelObject;

class LineLayoutBoxModel : public LineLayoutItem {
 public:
  explicit LineLayoutBoxModel(LayoutBoxModelObject* layout_box)
      : LineLayoutItem(layout_box) {}

  explicit LineLayoutBoxModel(const LineLayoutItem& item)
      : LineLayoutItem(item) {
    SECURITY_DCHECK(!item || item.IsBoxModelObject());
  }

  explicit LineLayoutBoxModel(std::nullptr_t) : LineLayoutItem(nullptr) {}

  LineLayoutBoxModel() = default;

  // TODO(dgrogan) Remove. Implement API methods that proxy to the PaintLayer.
  PaintLayer* Layer() const { return ToBoxModel()->Layer(); }

  LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode line_direction_mode,
      LinePositionMode line_position_mode = kPositionOnContainingLine) const {
    return ToBoxModel()->LineHeight(first_line, line_direction_mode,
                                    line_position_mode);
  }

  LayoutUnit BaselinePosition(
      FontBaseline font_baseline,
      bool first_line,
      LineDirectionMode line_direction_mode,
      LinePositionMode line_position_mode = kPositionOnContainingLine) const {
    return ToBoxModel()->BaselinePosition(
        font_baseline, first_line, line_direction_mode, line_position_mode);
  }

  bool HasSelfPaintingLayer() const {
    return ToBoxModel()->HasSelfPaintingLayer();
  }

  LayoutUnit MarginTop() const { return ToBoxModel()->MarginTop(); }

  LayoutUnit MarginBottom() const { return ToBoxModel()->MarginBottom(); }

  LayoutUnit MarginLeft() const { return ToBoxModel()->MarginLeft(); }

  LayoutUnit MarginRight() const { return ToBoxModel()->MarginRight(); }

  LayoutUnit MarginBefore(const ComputedStyle* other_style = nullptr) const {
    return ToBoxModel()->MarginBefore(other_style);
  }

  LayoutUnit MarginAfter(const ComputedStyle* other_style = nullptr) const {
    return ToBoxModel()->MarginAfter(other_style);
  }

  LayoutUnit MarginOver() const { return ToBoxModel()->MarginOver(); }

  LayoutUnit MarginUnder() const { return ToBoxModel()->MarginUnder(); }

  LayoutUnit PaddingTop() const { return ToBoxModel()->PaddingTop(); }

  LayoutUnit PaddingBottom() const { return ToBoxModel()->PaddingBottom(); }

  LayoutUnit PaddingLeft() const { return ToBoxModel()->PaddingLeft(); }

  LayoutUnit PaddingRight() const { return ToBoxModel()->PaddingRight(); }

  LayoutUnit PaddingBefore() const { return ToBoxModel()->PaddingBefore(); }

  LayoutUnit PaddingAfter() const { return ToBoxModel()->PaddingAfter(); }

  LayoutUnit BorderTop() const { return ToBoxModel()->BorderTop(); }

  LayoutUnit BorderBottom() const { return ToBoxModel()->BorderBottom(); }

  LayoutUnit BorderLeft() const { return ToBoxModel()->BorderLeft(); }

  LayoutUnit BorderRight() const { return ToBoxModel()->BorderRight(); }

  LayoutUnit BorderBefore() const { return ToBoxModel()->BorderBefore(); }

  LayoutUnit BorderAfter() const { return ToBoxModel()->BorderAfter(); }

  LayoutSize RelativePositionLogicalOffset() const {
    return ToBoxModel()->RelativePositionLogicalOffset();
  }

  bool HasInlineDirectionBordersOrPadding() const {
    return ToBoxModel()->HasInlineDirectionBordersOrPadding();
  }

  LayoutUnit BorderAndPaddingOver() const {
    return ToBoxModel()->BorderAndPaddingOver();
  }

  LayoutUnit BorderAndPaddingUnder() const {
    return ToBoxModel()->BorderAndPaddingUnder();
  }

  LayoutUnit BorderAndPaddingLogicalHeight() const {
    return ToBoxModel()->BorderAndPaddingLogicalHeight();
  }

  LayoutSize OffsetForInFlowPosition() const {
    return ToBoxModel()->OffsetForInFlowPosition();
  }

 private:
  LayoutBoxModelObject* ToBoxModel() {
    return ToLayoutBoxModelObject(GetLayoutObject());
  }
  const LayoutBoxModelObject* ToBoxModel() const {
    return ToLayoutBoxModelObject(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BOX_MODEL_H_
