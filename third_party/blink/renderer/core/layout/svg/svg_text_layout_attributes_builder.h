/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_LAYOUT_ATTRIBUTES_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_LAYOUT_ATTRIBUTES_BUILDER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/layout/svg/svg_character_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutBoxModelObject;
class LayoutSVGText;
class SVGTextPositioningElement;

// SVGTextLayoutAttributesBuilder performs the first layout phase for SVG text.
//
// It extracts the x/y/dx/dy/rotate values from the SVGTextPositioningElements
// in the DOM. These values are propagated to the corresponding
// LayoutSVGInlineText layoutObjects.
// The first layout phase only extracts the relevant information needed in
// LayoutBlockFlowLine to create the InlineBox tree based on text chunk
// boundaries & BiDi information.
// The second layout phase is carried out by SVGTextLayoutEngine.
class SVGTextLayoutAttributesBuilder {
  STACK_ALLOCATED();

 public:
  explicit SVGTextLayoutAttributesBuilder(LayoutSVGText&);

  void BuildLayoutAttributes();

  struct TextPosition {
    DISALLOW_NEW();

   public:
    TextPosition(SVGTextPositioningElement* new_element = nullptr,
                 unsigned new_start = 0,
                 unsigned new_length = 0)
        : element(new_element), start(new_start), length(new_length) {}

    void Trace(blink::Visitor*);

    Member<SVGTextPositioningElement> element;
    unsigned start;
    unsigned length;
  };

 private:
  void BuildCharacterDataMap(LayoutSVGText&);
  void BuildLayoutAttributes(LayoutSVGText&) const;
  void CollectTextPositioningElements(LayoutBoxModelObject&);
  void FillCharacterDataMap(const TextPosition&);

  LayoutSVGText& text_root_;
  unsigned character_count_;
  HeapVector<TextPosition> text_positions_;
  SVGCharacterDataMap character_data_map_;

  DISALLOW_COPY_AND_ASSIGN(SVGTextLayoutAttributesBuilder);
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(
    blink::SVGTextLayoutAttributesBuilder::TextPosition)

#endif
