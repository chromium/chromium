/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_attributes_builder.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/svg/svg_text_positioning_element.h"

namespace blink {

namespace {

void UpdateLayoutAttributes(LayoutSVGInlineText& text,
                            unsigned& value_list_position,
                            const SVGCharacterDataMap& all_characters_map) {
  SVGCharacterDataMap& character_data_map = text.CharacterDataMap();
  character_data_map.clear();

  LineLayoutSVGInlineText text_line_layout(&text);
  for (SVGInlineTextMetricsIterator iterator(text_line_layout);
       !iterator.IsAtEnd(); iterator.Next()) {
    if (iterator.Metrics().IsEmpty())
      continue;

    auto it = all_characters_map.find(value_list_position + 1);
    if (it != all_characters_map.end())
      character_data_map.Set(iterator.CharacterOffset() + 1, it->value);

    // Increase the position in the value/attribute list with one for each
    // "character unit" (that will be displayed.)
    value_list_position++;
  }
}

}  // namespace

SVGTextLayoutAttributesBuilder::SVGTextLayoutAttributesBuilder(
    LayoutSVGText& text_root)
    : text_root_(text_root), character_count_(0) {}

void SVGTextLayoutAttributesBuilder::BuildLayoutAttributes() {
  character_data_map_.clear();

  if (text_positions_.IsEmpty()) {
    character_count_ = 0;
    CollectTextPositioningElements(text_root_);
  }

  if (!character_count_)
    return;

  BuildCharacterDataMap(text_root_);

  unsigned value_list_position = 0;
  LayoutObject* child = text_root_.FirstChild();
  while (child) {
    if (child->IsSVGInlineText()) {
      UpdateLayoutAttributes(ToLayoutSVGInlineText(*child), value_list_position,
                             character_data_map_);
    } else if (child->IsSVGInline()) {
      // Visit children of text content elements.
      if (LayoutObject* inline_child = ToLayoutSVGInline(child)->FirstChild()) {
        child = inline_child;
        continue;
      }
    }
    child = child->NextInPreOrderAfterChildren(&text_root_);
  }
}

static inline unsigned CountCharactersInTextNode(
    const LayoutSVGInlineText& text) {
  unsigned num_characters = 0;
  for (const SVGTextMetrics& metrics : text.MetricsList()) {
    if (metrics.IsEmpty())
      continue;
    num_characters++;
  }
  return num_characters;
}

static SVGTextPositioningElement* PositioningElementFromLayoutObject(
    LayoutObject& layout_object) {
  DCHECK(layout_object.IsSVGText() || layout_object.IsSVGInline());

  Node* node = layout_object.GetNode();
  DCHECK(node);
  DCHECK(node->IsSVGElement());

  return DynamicTo<SVGTextPositioningElement>(node);
}

void SVGTextLayoutAttributesBuilder::CollectTextPositioningElements(
    LayoutBoxModelObject& start) {
  DCHECK(!start.IsSVGText() || text_positions_.IsEmpty());
  SVGTextPositioningElement* element =
      PositioningElementFromLayoutObject(start);
  unsigned at_position = text_positions_.size();
  if (element)
    text_positions_.push_back(TextPosition(element, character_count_));

  for (LayoutObject* child = start.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsSVGInlineText()) {
      character_count_ +=
          CountCharactersInTextNode(ToLayoutSVGInlineText(*child));
      continue;
    }

    if (child->IsSVGInline()) {
      CollectTextPositioningElements(ToLayoutSVGInline(*child));
      continue;
    }
  }

  if (!element)
    return;

  // Compute the length of the subtree after all children have been visited.
  TextPosition& position = text_positions_[at_position];
  DCHECK(!position.length);
  position.length = character_count_ - position.start;
}

void SVGTextLayoutAttributesBuilder::BuildCharacterDataMap(
    LayoutSVGText& text_root) {
  // Fill character data map using text positioning elements in top-down order.
  for (const TextPosition& position : text_positions_)
    FillCharacterDataMap(position);

  // Handle x/y default attributes.
  SVGCharacterData& data =
      character_data_map_.insert(1, SVGCharacterData()).stored_value->value;
  if (!data.HasX())
    data.x = 0;
  if (!data.HasY())
    data.y = 0;
}

namespace {

class AttributeListsIterator {
  STACK_ALLOCATED();

 public:
  AttributeListsIterator(SVGTextPositioningElement*);

  bool HasAttributes() const {
    return x_list_remaining_ || y_list_remaining_ || dx_list_remaining_ ||
           dy_list_remaining_ || rotate_list_remaining_;
  }
  void UpdateCharacterData(wtf_size_t index, SVGCharacterData&);

 private:
  SVGLengthContext length_context_;
  Member<SVGLengthList> x_list_;
  unsigned x_list_remaining_;
  Member<SVGLengthList> y_list_;
  unsigned y_list_remaining_;
  Member<SVGLengthList> dx_list_;
  unsigned dx_list_remaining_;
  Member<SVGLengthList> dy_list_;
  unsigned dy_list_remaining_;
  Member<SVGNumberList> rotate_list_;
  unsigned rotate_list_remaining_;
};

AttributeListsIterator::AttributeListsIterator(
    SVGTextPositioningElement* element)
    : length_context_(element),
      x_list_(element->x()->CurrentValue()),
      x_list_remaining_(x_list_->length()),
      y_list_(element->y()->CurrentValue()),
      y_list_remaining_(y_list_->length()),
      dx_list_(element->dx()->CurrentValue()),
      dx_list_remaining_(dx_list_->length()),
      dy_list_(element->dy()->CurrentValue()),
      dy_list_remaining_(dy_list_->length()),
      rotate_list_(element->rotate()->CurrentValue()),
      rotate_list_remaining_(rotate_list_->length()) {}

inline void AttributeListsIterator::UpdateCharacterData(
    wtf_size_t index,
    SVGCharacterData& data) {
  if (x_list_remaining_) {
    data.x = x_list_->at(index)->Value(length_context_);
    --x_list_remaining_;
  }
  if (y_list_remaining_) {
    data.y = y_list_->at(index)->Value(length_context_);
    --y_list_remaining_;
  }
  if (dx_list_remaining_) {
    data.dx = dx_list_->at(index)->Value(length_context_);
    --dx_list_remaining_;
  }
  if (dy_list_remaining_) {
    data.dy = dy_list_->at(index)->Value(length_context_);
    --dy_list_remaining_;
  }
  if (rotate_list_remaining_) {
    data.rotate =
        rotate_list_->at(std::min(index, rotate_list_->length() - 1))->Value();
    // The last rotation value spans the whole scope.
    if (rotate_list_remaining_ > 1)
      --rotate_list_remaining_;
  }
}

}  // namespace

void SVGTextLayoutAttributesBuilder::FillCharacterDataMap(
    const TextPosition& position) {
  AttributeListsIterator attr_lists(position.element);
  for (wtf_size_t i = 0; attr_lists.HasAttributes() && i < position.length;
       ++i) {
    SVGCharacterData& data =
        character_data_map_.insert(position.start + i + 1, SVGCharacterData())
            .stored_value->value;
    attr_lists.UpdateCharacterData(i, data);
  }
}

void SVGTextLayoutAttributesBuilder::TextPosition::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(element);
}

}  // namespace blink
