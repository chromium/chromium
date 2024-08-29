// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_attributes_builder.h"

#include <optional>

#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length_list.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number_list.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_text_positioning_element.h"

namespace blink {

struct SVGTextLengthContext {
  DISALLOW_NEW();

 public:
  void Trace(Visitor* visitor) const { visitor->Trace(layout_object); }

  Member<const LayoutObject> layout_object;
  unsigned start_index;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::SVGTextLengthContext)

namespace blink {

namespace {

// Iterates over x/y/dx/dy/rotate attributes on an SVGTextPositioningElement.
//
// This class is used only by LayoutAttributesStack.
// This class is not copyable.
class LayoutAttributesIterator final
    : public GarbageCollected<LayoutAttributesIterator> {
 public:
  LayoutAttributesIterator(const LayoutObject& layout_object, bool in_text_path)
      : element_(To<SVGTextPositioningElement>(layout_object.GetNode())),
        x_(element_->x()->CurrentValue()),
        y_(element_->y()->CurrentValue()),
        dx_(element_->dx()->CurrentValue()),
        dy_(element_->dy()->CurrentValue()),
        rotate_(element_->rotate()->CurrentValue()),
        in_text_path_(in_text_path) {}
  LayoutAttributesIterator(const LayoutAttributesIterator&) = delete;
  LayoutAttributesIterator& operator=(const LayoutAttributesIterator&) = delete;

  void Trace(Visitor* visitor) const {
    visitor->Trace(element_);
    visitor->Trace(x_);
    visitor->Trace(y_);
    visitor->Trace(dx_);
    visitor->Trace(dy_);
    visitor->Trace(rotate_);
  }

  bool HasX() const { return consumed_ < x_->length(); }
  bool HasY() const { return consumed_ < y_->length(); }
  bool HasDx() const { return consumed_ < dx_->length(); }
  bool HasDy() const { return consumed_ < dy_->length(); }

  float X() const {
    return x_->at(consumed_)->Value(SVGLengthContext(element_));
  }
  float Y() const {
    return y_->at(consumed_)->Value(SVGLengthContext(element_));
  }
  float Dx() const {
    return dx_->at(consumed_)->Value(SVGLengthContext(element_));
  }
  float Dy() const {
    return dy_->at(consumed_)->Value(SVGLengthContext(element_));
  }

  float MatchedOrLastRotate() const {
    uint32_t length = rotate_->length();
    if (length == 0) {
      return SvgCharacterData::EmptyValue();
    }
    if (consumed_ < length) {
      return rotate_->at(consumed_)->Value();
    }
    return rotate_->at(length - 1)->Value();
  }

  bool InTextPath() const { return in_text_path_; }

  // This function should be called whenever we handled an addressable
  // character in a descendant LayoutText.
  void Advance() { ++consumed_; }

 private:
  // The following six Member<>s never be null.
  const Member<const SVGTextPositioningElement> element_;
  const Member<const SVGLengthList> x_;
  const Member<const SVGLengthList> y_;
  const Member<const SVGLengthList> dx_;
  const Member<const SVGLengthList> dy_;
  const Member<const SVGNumberList> rotate_;
  const bool in_text_path_;
  // How many addressable characters in this element are consumed.
  unsigned consumed_ = 0;
};

// A stack of LayoutAttributesIterator.
// This class is not copyable.
class LayoutAttributesStack final {
  STACK_ALLOCATED();

 public:
  LayoutAttributesStack() = default;
  ~LayoutAttributesStack() { DCHECK_EQ(stack_.size(), 0u); }
  LayoutAttributesStack(const LayoutAttributesStack&) = delete;
  LayoutAttributesStack& operator=(const LayoutAttributesStack&) = delete;

  void Push(const LayoutObject& layout_object, bool in_text_path) {
    stack_.push_back(MakeGarbageCollected<LayoutAttributesIterator>(
        layout_object, in_text_path));
  }
  void Pop() { stack_.pop_back(); }

  // Advance all of iterators in the stack.
  void Advance() {
    DCHECK_GT(stack_.size(), 0u);
    for (auto& iterator : stack_) {
      iterator->Advance();
    }
  }

  // X(), Y(), Dx(), and Dy() return an effective 'x, 'y', 'dx', or 'dy' value,
  // or EmptyValue().

  float X() const {
    auto it = base::ranges::find_if(base::Reversed(stack_),
                                    &LayoutAttributesIterator::HasX);
    return it != stack_.rend() ? (*it)->X() : SvgCharacterData::EmptyValue();
  }
  float Y() const {
    auto it = base::ranges::find_if(base::Reversed(stack_),
                                    &LayoutAttributesIterator::HasY);
    return it != stack_.rend() ? (*it)->Y() : SvgCharacterData::EmptyValue();
  }
  float Dx() const {
    auto it = base::ranges::find_if(base::Reversed(stack_),
                                    &LayoutAttributesIterator::HasDx);
    return it != stack_.rend() ? (*it)->Dx() : SvgCharacterData::EmptyValue();
  }
  float Dy() const {
    auto it = base::ranges::find_if(base::Reversed(stack_),
                                    &LayoutAttributesIterator::HasDy);
    return it != stack_.rend() ? (*it)->Dy() : SvgCharacterData::EmptyValue();
  }

  float MatchedOrLastRotate() const {
    for (const auto& attrs : base::Reversed(stack_)) {
      float rotate = attrs->MatchedOrLastRotate();
      if (!SvgCharacterData::IsEmptyValue(rotate)) {
        return rotate;
      }
    }
    return SvgCharacterData::EmptyValue();
  }

  bool ShouldStartAnchoredChunk(bool horizontal) const {
    // According to the algorithm, the x/y attributes on the nearest
    // SVGTextPositioningElement should overwrite |anchored chunk| flag set by
    // ancestors.  It's incorrect.
    // https://github.com/w3c/svgwg/issues/839
    //
    // If the current position is not in a <textPath>, we should just check
    // existence of available x/y attributes in ancestors.
    // Otherwise, we should check available x/y attributes declared in the
    // <textPath> descendants.

    if (!stack_.back()->InTextPath()) {
      return !SvgCharacterData::IsEmptyValue(X()) ||
             !SvgCharacterData::IsEmptyValue(Y());
    }

    for (const auto& attrs : base::Reversed(stack_)) {
      if (!attrs->InTextPath()) {
        return false;
      }
      if (horizontal) {
        if (attrs->HasX()) {
          return true;
        }
      } else {
        if (attrs->HasY()) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  HeapVector<Member<LayoutAttributesIterator>> stack_;
};

bool HasUpdated(const SvgCharacterData& data) {
  return data.HasX() || data.HasY() || data.HasDx() || data.HasDy() ||
         data.HasRotate() || data.anchored_chunk;
}

bool HasValidTextLength(const LayoutObject& layout_object) {
  if (auto* element =
          DynamicTo<SVGTextContentElement>(layout_object.GetNode())) {
    if (element->TextLengthIsSpecifiedByUser()) {
      float text_length = element->textLength()->CurrentValue()->Value(
          SVGLengthContext(element));
      // text_length is 0.0 if the textLength attribute has an invalid
      // string. Legacy SVG <text> skips textLength processing if the
      // attribute is "0" or invalid. Firefox skips textLength processing if
      // textLength value is smaller than the intrinsic width of the text.
      // This code follows the legacy behavior.
      return text_length > 0.0f;
    }
  }
  return false;
}

}  // anonymous namespace

SvgTextLayoutAttributesBuilder::SvgTextLayoutAttributesBuilder(InlineNode ifc)
    : block_flow_(To<LayoutBlockFlow>(ifc.GetLayoutBox())) {}

// This is an implementation of "3. Resolve character positioning" in [1],
// without recursion.
//
// An SVGCharacterDataWithAnchor stores resolve_x, resolve_y, resolve_dx,
// resolve_dy, "rotate" of result[], and "anchored chunk" of result[].
//
// [1]: https://svgwg.org/svg2-draft/text.html#TextLayoutAlgorithm
void SvgTextLayoutAttributesBuilder::Build(
    const String& ifc_text_content,
    const HeapVector<InlineItem>& items) {
  LayoutAttributesStack attr_stack;
  HeapVector<SVGTextLengthContext> text_length_stack;
  unsigned addressable_index = 0;
  bool is_first_char = true;
  bool in_text_path = false;
  std::optional<unsigned> text_path_start;
  bool first_char_in_text_path = false;
  const bool horizontal = block_flow_->IsHorizontalWritingMode();

  attr_stack.Push(*block_flow_, in_text_path);
  if (HasValidTextLength(*block_flow_)) {
    text_length_stack.push_back(
        SVGTextLengthContext{block_flow_, addressable_index});
  }
  for (const auto& item : items) {
    const LayoutObject* object = item.GetLayoutObject();

    if (item.Type() == InlineItem::kOpenTag) {
      if (object->IsSVGTSpan()) {
        attr_stack.Push(*object, in_text_path);
      } else if (object->IsSVGTextPath()) {
        // 2.2. Set the "anchored chunk" flag of result[index] to true.
        first_char_in_text_path = true;
        // 2.3. Set in_text_path flag true.
        in_text_path = true;
        text_path_start = addressable_index;
      }
      if (HasValidTextLength(*object)) {
        text_length_stack.push_back(
            SVGTextLengthContext{object, addressable_index});
      }

    } else if (item.Type() == InlineItem::kCloseTag) {
      if (object->IsSVGTSpan()) {
        attr_stack.Pop();
      } else if (object->IsSVGTextPath()) {
        // 4.1. Set "in_text_path" flag false.
        // According to the specification, <textPath> can't be nested.
        in_text_path = false;
        first_char_in_text_path = false;
        DCHECK(text_path_start);
        if (addressable_index != *text_path_start) {
          text_path_range_list_.push_back(SvgTextContentRange{
              object, *text_path_start, addressable_index - 1});
        }
        text_path_start.reset();
      }
      if (text_length_stack.size() > 0u &&
          text_length_stack.back().layout_object == object) {
        if (text_length_stack.back().start_index != addressable_index) {
          text_length_range_list_.push_back(
              SvgTextContentRange{object, text_length_stack.back().start_index,
                                  addressable_index - 1});
        }
        text_length_stack.pop_back();
      }

    } else if (item.Type() != InlineItem::kText) {
      continue;
    }

    StringView item_string(ifc_text_content, item.StartOffset(), item.Length());
    for (unsigned i = 0; i < item.Length();) {
      SvgCharacterData data;

      // 2.2. Set the "anchored chunk" flag of result[index] to true.
      // 1.6.1.1. If i < new_check_count, then set the "anchored chunk" flag
      // of result[index + j] to true. Else set the flag to false.
      if (first_char_in_text_path) {
        data.anchored_chunk = true;
      } else {
        data.anchored_chunk = attr_stack.ShouldStartAnchoredChunk(horizontal);
      }

      // 1.6.1.2. If i < length of x, then set resolve_x[index + j] to x[i].
      data.x = attr_stack.X();

      // 1.6.1.3. If "in_text_path" flag is true and the "horizontal" flag is
      // false, unset resolve_x[index].
      if (in_text_path && !horizontal) {
        data.x = SvgCharacterData::EmptyValue();
      }
      // Not in the specification; Set X of the first character in a
      // <textPath> to 0 in order to:
      //   - Reset dx in AdjustPositionsDxDy().
      //   - Anchor at 0 in ApplyAnchoring().
      // https://github.com/w3c/svgwg/issues/274
      if (first_char_in_text_path && horizontal && !data.HasX()) {
        data.x = 0.0f;
      }

      // 1.6.1.4. If i < length of y, then set resolve_y[index + j] to y[i].
      data.y = attr_stack.Y();

      // 1.6.1.5. If "in_text_path" flag is true and the "horizontal" flag is
      // true, unset resolve_y[index].
      if (in_text_path && horizontal) {
        data.y = SvgCharacterData::EmptyValue();
      }
      // Not in the specification; Set Y of the first character in a
      // <textPath> to 0 in order to:
      //   - Reset dy in AdjustPositionsDxDy().
      //   - Anchor at 0 in ApplyAnchoring().
      // https://github.com/w3c/svgwg/issues/274
      if (first_char_in_text_path && !horizontal && !data.HasY()) {
        data.y = 0.0f;
      }

      first_char_in_text_path = false;

      // Not in the specification; The following code sets the initial inline
      // offset of 'current text position' to 0.
      // See InlineLayoutAlgorithm::CreateLine() for the initial block offset.
      if (is_first_char) {
        is_first_char = false;
        if (horizontal) {
          if (!data.HasX()) {
            data.x = 0.0f;
          }
        } else {
          if (!data.HasY()) {
            data.y = 0.0f;
          }
        }
      }

      // 1.6.1.6. If i < length of dx, then set resolve_dx[index + j] to dx[i].
      data.dx = attr_stack.Dx();

      // 1.6.1.7. If i < length of dy, then set resolve_dy[index + j] to dy[i].
      data.dy = attr_stack.Dy();

      // 1.6.1.8. If i < length of rotate, then set the angle value of
      // result[index + j] to rotate[i]. Otherwise, if rotate is not empty,
      // then set result[index + j] to result[index + j − 1].
      data.rotate = attr_stack.MatchedOrLastRotate();

      if (HasUpdated(data)) {
        resolved_.push_back(std::make_pair(addressable_index, data));
        ifc_text_content_offsets_.push_back(item.StartOffset() + i);
      }
      ++addressable_index;
      attr_stack.Advance();
      i = item_string.NextCodePointOffset(i);
    }
  }
  if (text_length_stack.size() > 0u) {
    DCHECK_EQ(text_length_stack.back().layout_object, block_flow_);
    DCHECK_EQ(text_length_stack.back().start_index, 0u);
    text_length_range_list_.push_back(
        SvgTextContentRange{block_flow_, 0u, addressable_index - 1});
    text_length_stack.pop_back();
  }
  attr_stack.Pop();
  DCHECK_EQ(resolved_.size(), ifc_text_content_offsets_.size());
}

SvgInlineNodeData* SvgTextLayoutAttributesBuilder::CreateSvgInlineNodeData() {
  auto* svg_node_data = MakeGarbageCollected<SvgInlineNodeData>();
  svg_node_data->character_data_list = std::move(resolved_);
  svg_node_data->text_length_range_list = std::move(text_length_range_list_);
  svg_node_data->text_path_range_list = std::move(text_path_range_list_);
  return svg_node_data;
}

unsigned SvgTextLayoutAttributesBuilder::IfcTextContentOffsetAt(
    wtf_size_t index) {
  return ifc_text_content_offsets_.at(index);
}

}  // namespace blink
