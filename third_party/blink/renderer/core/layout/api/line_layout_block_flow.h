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

  LayoutUnit StartAlignedOffsetForLine(LayoutUnit position,
                                       IndentTextOrNot indent_text) {
    return ToBlockFlow()->StartAlignedOffsetForLine(position, indent_text);
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

  void SetStaticInlinePositionForChild(LineLayoutBox box,
                                       LayoutUnit inline_position) {
    ToBlockFlow()->SetStaticInlinePositionForChild(
        *To<LayoutBox>(box.GetLayoutObject()), inline_position);
  }

  void UpdateStaticInlinePositionForChild(
      LineLayoutBox box,
      LayoutUnit logical_top,
      IndentTextOrNot indent_text = kDoNotIndentText) {
    ToBlockFlow()->UpdateStaticInlinePositionForChild(
        *To<LayoutBox>(box.GetLayoutObject()), logical_top, indent_text);
  }

  LayoutUnit LogicalRightOffsetForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    return ToBlockFlow()->LogicalRightOffsetForLine(position, indent_text,
                                                    logical_height);
  }

  LayoutUnit LogicalLeftOffsetForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    return ToBlockFlow()->LogicalLeftOffsetForLine(position, indent_text,
                                                   logical_height);
  }

  LayoutUnit LogicalWidth() { return ToBlockFlow()->LogicalWidth(); }

  LineBoxList* LineBoxes() { return ToBlockFlow()->LineBoxes(); }

  InlineBox* CreateAndAppendRootInlineBox() {
    return ToBlockFlow()->CreateAndAppendRootInlineBox();
  }

  InlineFlowBox* LastLineBox() { return ToBlockFlow()->LastLineBox(); }

  InlineFlowBox* FirstLineBox() { return ToBlockFlow()->FirstLineBox(); }

  RootInlineBox* FirstRootBox() const { return ToBlockFlow()->FirstRootBox(); }

  RootInlineBox* LastRootBox() const { return ToBlockFlow()->LastRootBox(); }

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
