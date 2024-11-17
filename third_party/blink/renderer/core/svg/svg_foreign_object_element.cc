/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

SVGForeignObjectElement::SVGForeignObjectElement(Document& document)
    : SVGGraphicsElement(svg_names::kForeignObjectTag, document),
      x_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kXAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kX)),
      y_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kYAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kY)),
      width_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kWidthAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kWidth)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kHeight)) {
  UseCounter::Count(document, WebFeature::kSVGForeignObjectElement);
}

void SVGForeignObjectElement::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  SVGGraphicsElement::Trace(visitor);
}

void SVGForeignObjectElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  bool is_width_height_attribute =
      attr_name == svg_names::kWidthAttr || attr_name == svg_names::kHeightAttr;
  bool is_xy_attribute =
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr;

  if (is_xy_attribute || is_width_height_attribute) {
    UpdatePresentationAttributeStyle(params.property);
    UpdateRelativeLengthsInformation();
    if (LayoutObject* layout_object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(*layout_object);

    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(params);
}

LayoutObject* SVGForeignObjectElement::CreateLayoutObject(
    const ComputedStyle& style) {
  // Suppress foreignObject LayoutObjects in SVG hidden containers.
  // LayoutSVGHiddenContainers does not allow the subtree to be rendered, but
  // allow LayoutObject descendants to be created. That will causes crashes in
  // the layout code if object creation is not inhibited for foreignObject
  // subtrees (https://crbug.com/1027905).
  // Note that we currently do not support foreignObject instantiation via
  // <use>, and attachShadow is not allowed on SVG elements, hence it is safe to
  // use parentElement() here.
  for (Element* ancestor = parentElement();
       ancestor && ancestor->IsSVGElement();
       ancestor = ancestor->parentElement()) {
    if (ancestor->GetLayoutObject() &&
        ancestor->GetLayoutObject()->IsSVGHiddenContainer())
      return nullptr;
  }
  return MakeGarbageCollected<LayoutSVGForeignObject>(this);
}

bool SVGForeignObjectElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

SVGAnimatedPropertyBase* SVGForeignObjectElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kWidthAttr) {
    return width_.Get();
  } else if (attribute_name == svg_names::kHeightAttr) {
    return height_.Get();
  } else {
    return SVGGraphicsElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGForeignObjectElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x_.Get(), y_.Get(), width_.Get(),
                                   height_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGGraphicsElement::SynchronizeAllSVGAttributes();
}

void SVGForeignObjectElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  auto pres_attrs = std::to_array<const SVGAnimatedPropertyBase*>(
      {x_.Get(), y_.Get(), width_.Get(), height_.Get()});
  AddAnimatedPropertiesToPresentationAttributeStyle(pres_attrs, style);
  SVGGraphicsElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink
