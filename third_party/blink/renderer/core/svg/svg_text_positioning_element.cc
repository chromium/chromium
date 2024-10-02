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
#include "third_party/blink/renderer/core/svg/svg_animated_length_list.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number_list.h"
#include "third_party/blink/renderer/core/svg/svg_length_list.h"
#include "third_party/blink/renderer/core/svg/svg_number_list.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGTextPositioningElement::SVGTextPositioningElement(
    const QualifiedName& tag_name,
    Document& document)
    : SVGTextContentElement(tag_name, document),
      x_(MakeGarbageCollected<SVGAnimatedLengthList>(
          this,
          svg_names::kXAttr,
          MakeGarbageCollected<SVGLengthList>(SVGLengthMode::kWidth))),
      y_(MakeGarbageCollected<SVGAnimatedLengthList>(
          this,
          svg_names::kYAttr,
          MakeGarbageCollected<SVGLengthList>(SVGLengthMode::kHeight))),
      dx_(MakeGarbageCollected<SVGAnimatedLengthList>(
          this,
          svg_names::kDxAttr,
          MakeGarbageCollected<SVGLengthList>(SVGLengthMode::kWidth))),
      dy_(MakeGarbageCollected<SVGAnimatedLengthList>(
          this,
          svg_names::kDyAttr,
          MakeGarbageCollected<SVGLengthList>(SVGLengthMode::kHeight))),
      rotate_(MakeGarbageCollected<SVGAnimatedNumberList>(
          this,
          svg_names::kRotateAttr)) {}

void SVGTextPositioningElement::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(dx_);
  visitor->Trace(dy_);
  visitor->Trace(rotate_);
  SVGTextContentElement::Trace(visitor);
}

void SVGTextPositioningElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  bool update_relative_lengths =
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kDxAttr || attr_name == svg_names::kDyAttr;

  if (update_relative_lengths)
    UpdateRelativeLengthsInformation();

  if (update_relative_lengths || attr_name == svg_names::kRotateAttr) {
    LayoutObject* layout_object = GetLayoutObject();
    if (!layout_object)
      return;

    if (auto* ng_text =
            LayoutSVGText::LocateLayoutSVGTextAncestor(layout_object)) {
      ng_text->SetNeedsPositioningValuesUpdate();
    }
    MarkForLayoutAndParentResourceInvalidation(*layout_object);
    return;
  }

  SVGTextContentElement::SvgAttributeChanged(params);
}

SVGAnimatedPropertyBase* SVGTextPositioningElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kDxAttr) {
    return dx_.Get();
  } else if (attribute_name == svg_names::kDyAttr) {
    return dy_.Get();
  } else if (attribute_name == svg_names::kRotateAttr) {
    return rotate_.Get();
  } else {
    return SVGTextContentElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGTextPositioningElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x_.Get(), y_.Get(), dx_.Get(), dy_.Get(),
                                   rotate_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGTextContentElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
