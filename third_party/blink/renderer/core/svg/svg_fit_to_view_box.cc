/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_fit_to_view_box.h"

#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

class SVGAnimatedViewBoxRect : public SVGAnimatedRect {
 public:
  SVGAnimatedViewBoxRect(SVGElement* context_element)
      : SVGAnimatedRect(context_element, svg_names::kViewBoxAttr) {}

  SVGParsingError AttributeChanged(const String&) override;
};

SVGParsingError SVGAnimatedViewBoxRect::AttributeChanged(const String& value) {
  SVGParsingError parse_status = SVGAnimatedRect::AttributeChanged(value);

  if (parse_status == SVGParseStatus::kNoError &&
      (BaseValue()->Width() < 0 || BaseValue()->Height() < 0)) {
    parse_status = SVGParseStatus::kNegativeValue;
    BaseValue()->SetInvalid();
  }
  return parse_status;
}

SVGFitToViewBox::SVGFitToViewBox(SVGElement* element)
    : view_box_(MakeGarbageCollected<SVGAnimatedViewBoxRect>(element)),
      preserve_aspect_ratio_(
          MakeGarbageCollected<SVGAnimatedPreserveAspectRatio>(
              element,
              svg_names::kPreserveAspectRatioAttr)) {
  DCHECK(element);
  element->AddToPropertyMap(view_box_);
  element->AddToPropertyMap(preserve_aspect_ratio_);
}

void SVGFitToViewBox::Trace(blink::Visitor* visitor) {
  visitor->Trace(view_box_);
  visitor->Trace(preserve_aspect_ratio_);
}

AffineTransform SVGFitToViewBox::ViewBoxToViewTransform(
    const FloatRect& view_box_rect,
    const SVGPreserveAspectRatio* preserve_aspect_ratio,
    float view_width,
    float view_height) {
  if (!view_box_rect.Width() || !view_box_rect.Height() || !view_width ||
      !view_height)
    return AffineTransform();

  return preserve_aspect_ratio->ComputeTransform(
      view_box_rect.X(), view_box_rect.Y(), view_box_rect.Width(),
      view_box_rect.Height(), view_width, view_height);
}

bool SVGFitToViewBox::IsKnownAttribute(const QualifiedName& attr_name) {
  return attr_name == svg_names::kViewBoxAttr ||
         attr_name == svg_names::kPreserveAspectRatioAttr;
}

}  // namespace blink
