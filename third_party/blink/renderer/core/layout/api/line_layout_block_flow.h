// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BLOCK_FLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_API_LINE_LAYOUT_BLOCK_FLOW_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/floating_objects.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutBlockFlow;
class FloatingObject;
class LineWidth;

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
        *ToLayoutBox(child.GetLayoutObject()));
  }

  LayoutUnit MarginStartForChild(const LayoutBoxModelObject& child) const {
    return ToBlockFlow()->MarginStartForChild(child);
  }

  LayoutUnit MarginStartForChild(LineLayoutBox child) const {
    return ToBlockFlow()->MarginStartForChild(
        *ToLayoutBoxModelObject(child.GetLayoutObject()));
  }

  LayoutUnit MarginEndForChild(const LayoutBoxModelObject& child) const {
    return ToBlockFlow()->MarginEndForChild(child);
  }

  LayoutUnit MarginEndForChild(LineLayoutBox child) const {
    return ToBlockFlow()->MarginEndForChild(
        *ToLayoutBoxModelObject(child.GetLayoutObject()));
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
        *ToLayoutBox(box.GetLayoutObject()), inline_position);
  }

  void UpdateStaticInlinePositionForChild(
      LineLayoutBox box,
      LayoutUnit logical_top,
      IndentTextOrNot indent_text = kDoNotIndentText) {
    ToBlockFlow()->UpdateStaticInlinePositionForChild(
        *ToLayoutBox(box.GetLayoutObject()), logical_top, indent_text);
  }

  FloatingObject* InsertFloatingObject(LayoutBox& box) {
    return ToBlockFlow()->InsertFloatingObject(box);
  }

  FloatingObject* InsertFloatingObject(LineLayoutBox box) {
    return ToBlockFlow()->InsertFloatingObject(
        *ToLayoutBox(box.GetLayoutObject()));
  }

  FloatingObject* LastPlacedFloat(
      FloatingObjectSetIterator* iterator = nullptr) const {
    return ToBlockFlow()->LastPlacedFloat(iterator);
  }

  bool PlaceNewFloats(LayoutUnit logical_top_margin_edge, LineWidth* width) {
    return ToBlockFlow()->PlaceNewFloats(logical_top_margin_edge, width);
  }

  void PositionAndLayoutFloat(FloatingObject& floating_object,
                              LayoutUnit logical_top_margin_edge) {
    ToBlockFlow()->PositionAndLayoutFloat(floating_object,
                                          logical_top_margin_edge);
  }

  LayoutUnit NextFloatLogicalBottomBelow(LayoutUnit logical_height) const {
    return ToBlockFlow()->NextFloatLogicalBottomBelow(logical_height);
  }

  FloatingObject* LastFloatFromPreviousLine() const {
    return ToBlockFlow()->LastFloatFromPreviousLine();
  }

  // TODO(dgrogan/eae): *ForFloat: add these methods to the FloatingObject
  // class. Be consistent with use of start/end/before/after instead of
  // logicalTop/Left etc.
  LayoutUnit LogicalTopForFloat(const FloatingObject& floating_object) const {
    return ToBlockFlow()->LogicalTopForFloat(floating_object);
  }

  LayoutUnit LogicalBottomForFloat(
      const FloatingObject& floating_object) const {
    return ToBlockFlow()->LogicalBottomForFloat(floating_object);
  }

  LayoutUnit LogicalLeftForFloat(const FloatingObject& floating_object) const {
    return ToBlockFlow()->LogicalLeftForFloat(floating_object);
  }

  LayoutUnit LogicalRightForFloat(const FloatingObject& floating_object) const {
    return ToBlockFlow()->LogicalRightForFloat(floating_object);
  }

  LayoutUnit LogicalWidthForFloat(const FloatingObject& floating_object) const {
    return ToBlockFlow()->LogicalWidthForFloat(floating_object);
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

  void SetHasMarkupTruncation(bool b) {
    ToBlockFlow()->SetHasMarkupTruncation(b);
  }

  LayoutUnit LogicalWidth() { return ToBlockFlow()->LogicalWidth(); }

  LineBoxList* LineBoxes() { return ToBlockFlow()->LineBoxes(); }

  bool ContainsFloats() const { return ToBlockFlow()->ContainsFloats(); }

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
