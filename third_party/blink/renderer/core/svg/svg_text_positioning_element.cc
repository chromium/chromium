/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_text_positioning_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/svg/svg_length_list.h"
#include "third_party/blink/renderer/core/svg/svg_number_list.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

SVGTextPositioningElement::SVGTextPositioningElement(
    const QualifiedName& tag_name,
    Document& document)
    : SVGTextContentElement(tag_name, document),
      x_(SVGAnimatedLengthList::Create(
          this,
          svg_names::kXAttr,
          SVGLengthList::Create(SVGLengthMode::kWidth))),
      y_(SVGAnimatedLengthList::Create(
          this,
          svg_names::kYAttr,
          SVGLengthList::Create(SVGLengthMode::kHeight))),
      dx_(SVGAnimatedLengthList::Create(
          this,
          svg_names::kDxAttr,
          SVGLengthList::Create(SVGLengthMode::kWidth))),
      dy_(SVGAnimatedLengthList::Create(
          this,
          svg_names::kDyAttr,
          SVGLengthList::Create(SVGLengthMode::kHeight))),
      rotate_(SVGAnimatedNumberList::Create(this, svg_names::kRotateAttr)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(dx_);
  AddToPropertyMap(dy_);
  AddToPropertyMap(rotate_);
}

void SVGTextPositioningElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(dx_);
  visitor->Trace(dy_);
  visitor->Trace(rotate_);
  SVGTextContentElement::Trace(visitor);
}

void SVGTextPositioningElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  bool update_relative_lengths =
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kDxAttr || attr_name == svg_names::kDyAttr;

  if (update_relative_lengths)
    UpdateRelativeLengthsInformation();

  if (update_relative_lengths || attr_name == svg_names::kRotateAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    LayoutObject* layout_object = GetLayoutObject();
    if (!layout_object)
      return;

    if (LayoutSVGText* text_layout_object =
            LayoutSVGText::LocateLayoutSVGTextAncestor(layout_object))
      text_layout_object->SetNeedsPositioningValuesUpdate();
    MarkForLayoutAndParentResourceInvalidation(*layout_object);
    return;
  }

  SVGTextContentElement::SvgAttributeChanged(attr_name);
}

}  // namespace blink
