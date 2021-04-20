// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_text_layout_attributes_builder.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

namespace blink {

namespace {

// TODO(tkent): Implement this.
class LayoutAttributesStack final {
  STACK_ALLOCATED();

 public:
  LayoutAttributesStack() = default;
  ~LayoutAttributesStack() = default;
  LayoutAttributesStack(const LayoutAttributesStack&) = delete;
  LayoutAttributesStack& operator=(const LayoutAttributesStack&) = delete;

  void Push(const LayoutObject& layout_object, bool in_text_path) {}
  void Pop() {}

  // Advance all of iterators in the stack.
  void Advance() {}

  // X(), Y(), Dx(), and Dy() return an effective 'x, 'y', 'dx', or 'dy' value,
  // or EmptyValue().

  float X() const { return SVGCharacterData::EmptyValue(); }
  float Y() const { return SVGCharacterData::EmptyValue(); }
  float Dx() const { return SVGCharacterData::EmptyValue(); }
  float Dy() const { return SVGCharacterData::EmptyValue(); }

  float MatchedOrLastRotate() const { return SVGCharacterData::EmptyValue(); }

  bool ShouldStartAnchoredChunk(bool horizontal) const { return false; }
};

bool HasUpdated(const NGSVGCharacterData& data) {
  return data.HasX() || data.HasY() || data.HasDx() || data.HasDy() ||
         data.HasRotate() || data.anchored_chunk;
}

}  // anonymous namespace

NGSVGTextLayoutAttributesBuilder::NGSVGTextLayoutAttributesBuilder(
    NGInlineNode ifc)
    : block_flow_(To<LayoutBlockFlow>(ifc.GetLayoutBox())) {}

// This is an implementation of "3. Resolve character positioning" in [1],
// without recursion.
//
// An SVGCharacterDataWithAnchor stores resolve_x, resolve_y, resolve_dx,
// resolve_dy, "rotate" of result[], and "anchored chunk" of result[].
//
// [1]: https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm
void NGSVGTextLayoutAttributesBuilder::Build(
    const String& ifc_text_content,
    const HeapVector<NGInlineItem>& items) {
  LayoutAttributesStack attr_stack;
  unsigned addressable_index = 0;
  bool in_text_path = false;
  bool first_char_in_text_path = false;
  const bool horizontal =
      IsHorizontalWritingMode(block_flow_->StyleRef().GetWritingMode());

  attr_stack.Push(*block_flow_, in_text_path);
  for (const auto& item : items) {
    const LayoutObject* object = item.GetLayoutObject();

    if (item.Type() == NGInlineItem::kOpenTag) {
      if (object->IsSVGTSpan()) {
        attr_stack.Push(*object, in_text_path);
      } else if (object->IsSVGTextPath()) {
        // 2.2. Set the "anchored chunk" flag of result[index] to true.
        first_char_in_text_path = true;
        // 2.3. Set in_text_path flag true.
        in_text_path = true;
      }

    } else if (item.Type() == NGInlineItem::kCloseTag) {
      if (object->IsSVGTSpan()) {
        attr_stack.Pop();
      } else if (object->IsSVGTextPath()) {
        // 4.1. Set "in_text_path" flag false.
        // According to the specification, <textPath> can't be nested.
        in_text_path = false;
        first_char_in_text_path = false;
      }

    } else if (item.Type() != NGInlineItem::kText) {
      continue;
    }

    StringView item_string(ifc_text_content, item.StartOffset(), item.Length());
    for (unsigned i = 0; i < item.Length();) {
      NGSVGCharacterData data;

      // 2.2. Set the "anchored chunk" flag of result[index] to true.
      // 1.6.1.1. If i < new_check_count, then set the "anchored chunk" flag
      // of result[index + j] to true. Else set the flag to false.
      if (first_char_in_text_path) {
        data.anchored_chunk = true;
        first_char_in_text_path = false;
      } else {
        data.anchored_chunk = attr_stack.ShouldStartAnchoredChunk(horizontal);
      }

      // 1.6.1.2. If i < length of x, then set resolve_x[index + j] to x[i].
      data.x = attr_stack.X();

      // 1.6.1.3. If "in_text_path" flag is true and the "horizontal" flag is
      // false, unset resolve_x[index].
      if (in_text_path && !horizontal)
        data.x = SVGCharacterData::EmptyValue();

      // 1.6.1.4. If i < length of y, then set resolve_y[index + j] to y[i].
      data.y = attr_stack.Y();

      // 1.6.1.5. If "in_text_path" flag is true and the "horizontal" flag is
      // true, unset resolve_y[index].
      if (in_text_path && horizontal)
        data.y = SVGCharacterData::EmptyValue();

      // 1.6.1.6. If i < length of dx, then set resolve_dx[index + j] to dx[i].
      data.dx = attr_stack.Dx();

      // 1.6.1.7. If i < length of dy, then set resolve_dy[index + j] to dy[i].
      data.dy = attr_stack.Dy();

      // 1.6.1.8. If i < length of rotate, then set the angle value of
      // result[index + j] to rotate[i]. Otherwise, if rotate is not empty,
      // then set result[index + j] to result[index + j − 1].
      data.rotate = attr_stack.MatchedOrLastRotate();

      if (HasUpdated(data))
        resolved_.push_back(std::make_pair(addressable_index, data));
      ++addressable_index;
      attr_stack.Advance();
      i += i + 1 < item.Length() && U16_IS_LEAD(item_string[i]) &&
                   U16_IS_TRAIL(item_string[i + 1])
               ? 2
               : 1;
    }
  }
  attr_stack.Pop();
}

}  // namespace blink
