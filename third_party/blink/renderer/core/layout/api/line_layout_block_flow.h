// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BLOCK_FLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BLOCK_FLOW_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutBlockFlow;

class LineLayoutBlockFlow : public LineLayoutBox {
 public:
  explicit LineLayoutBlockFlow(LayoutBlockFlow* block_flow)
      : LineLayoutBox(block_flow) {}

  explicit LineLayoutBlockFlow(const LineLayoutItem& item)
      : LineLayoutBox(item) {
    SECURITY_DCHECK(!item || item.IsLayoutBlockFlow());
  }

  explicit LineLayoutBlockFlow(std::nullptr_t) : LineLayoutBox(nullptr) {}

  LineLayoutBlockFlow() = default;

  LineLayoutItem FirstChild() const {
    return LineLayoutItem(ToBlockFlow()->FirstChild());
  }
  LineLayoutItem LastChild() const {
    return LineLayoutItem(ToBlockFlow()->LastChild());
  }

  bool CanContainFirstFormattedLine() const {
    return ToBlockFlow()->CanContainFirstFormattedLine();
  }

  LayoutUnit TextIndentOffset() const {
    return ToBlockFlow()->TextIndentOffset();
  }

  // TODO(dgrogan/eae): *ForChild methods: callers should call
  // child.logicalWidth etc, and the API should access the parent BlockFlow.
  LayoutUnit LogicalWidthForChild(const LayoutBox& child) const {
    return ToBlockFlow()->LogicalWidthForChild(child);
  }

  LayoutUnit LogicalWidthForChild(LineLayoutBox child) const {
    return ToBlockFlow()->LogicalWidthForChild(
        *To<LayoutBox>(child.GetLayoutObject()));
  }

  LayoutUnit MarginStartForChild(const LayoutBoxModelObject& child) const {
    return ToBlockFlow()->MarginStartForChild(child);
  }

  LayoutUnit MarginStartForChild(LineLayoutBox child) const {
    return ToBlockFlow()->MarginStartForChild(
        *To<LayoutBoxModelObject>(child.GetLayoutObject()));
  }

  LayoutUnit MarginEndForChild(const LayoutBoxModelObject& child) const {
    return ToBlockFlow()->MarginEndForChild(child);
  }

  LayoutUnit MarginEndForChild(LineLayoutBox child) const {
    return ToBlockFlow()->MarginEndForChild(
        *To<LayoutBoxModelObject>(child.GetLayoutObject()));
  }

  LayoutUnit MarginBeforeForChild(const LayoutBoxModelObject& child) const {
    return ToBlockFlow()->MarginBeforeForChild(child);
  }

  LayoutUnit StartOffsetForContent() const {
    return ToBlockFlow()->StartOffsetForContent();
  }

  LayoutUnit LineHeight(bool first_line,
                        LineDirectionMode direction,
                        LinePositionMode line_position_mode) const {
    return ToBlockFlow()->LineHeight(first_line, direction, line_position_mode);
  }

  LayoutUnit MinLineHeightForReplacedObject(bool is_first_line,
                                            LayoutUnit replaced_height) const {
    return ToBlockFlow()->MinLineHeightForReplacedObject(is_first_line,
                                                         replaced_height);
  }

  LayoutUnit LogicalWidth() { return ToBlockFlow()->LogicalWidth(); }

 private:
  LayoutBlockFlow* ToBlockFlow() {
    return To<LayoutBlockFlow>(GetLayoutObject());
  }
  const LayoutBlockFlow* ToBlockFlow() const {
    return To<LayoutBlockFlow>(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BLOCK_FLOW_H_
