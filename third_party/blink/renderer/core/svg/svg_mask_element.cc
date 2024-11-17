/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_mask_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGMaskElement::SVGMaskElement(Document& document)
    : SVGElement(svg_names::kMaskTag, document),
      SVGTests(this),
      // Spec: If the x/y attribute is not specified, the effect is as if a
      // value of "-10%" were specified.
      x_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kXAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercentMinus10,
          CSSPropertyID::kX)),
      y_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kYAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercentMinus10,
          CSSPropertyID::kY)),
      // Spec: If the width/height attribute is not specified, the effect is as
      // if a value of "120%" were specified.
      width_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kWidthAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercent120,
          CSSPropertyID::kWidth)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercent120,
          CSSPropertyID::kHeight)),
      mask_units_(MakeGarbageCollected<
                  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kMaskUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)),
      mask_content_units_(MakeGarbageCollected<
                          SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kMaskContentUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {}

void SVGMaskElement::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(mask_units_);
  visitor->Trace(mask_content_units_);
  SVGElement::Trace(visitor);
  SVGTests::Trace(visitor);
}

void SVGMaskElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  bool is_length_attr =
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr || attr_name == svg_names::kHeightAttr;

  if (is_length_attr || attr_name == svg_names::kMaskUnitsAttr ||
      attr_name == svg_names::kMaskContentUnitsAttr ||
      SVGTests::IsKnownAttribute(attr_name)) {
    if (is_length_attr) {
      UpdatePresentationAttributeStyle(params.property);
      UpdateRelativeLengthsInformation();
    }

    auto* layout_object = To<LayoutSVGResourceContainer>(GetLayoutObject());
    if (layout_object) {
      layout_object->InvalidateCache();
    }
    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

void SVGMaskElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (change.ByParser())
    return;

  auto* layout_object = To<LayoutSVGResourceContainer>(GetLayoutObject());
  if (layout_object) {
    layout_object->InvalidateCache();
  }
}

LayoutObject* SVGMaskElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGResourceMasker>(this);
}

bool SVGMaskElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

SVGAnimatedPropertyBase* SVGMaskElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kWidthAttr) {
    return width_.Get();
  } else if (attribute_name == svg_names::kHeightAttr) {
    return height_.Get();
  } else if (attribute_name == svg_names::kMaskUnitsAttr) {
    return mask_units_.Get();
  } else if (attribute_name == svg_names::kMaskContentUnitsAttr) {
    return mask_content_units_.Get();
  } else {
    SVGAnimatedPropertyBase* ret;
    if (ret = SVGTests::PropertyFromAttribute(attribute_name); ret) {
      return ret;
    }
    return SVGElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGMaskElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{
      x_.Get(),      y_.Get(),          width_.Get(),
      height_.Get(), mask_units_.Get(), mask_content_units_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGTests::SynchronizeAllSVGAttributes();
  SVGElement::SynchronizeAllSVGAttributes();
}

void SVGMaskElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  auto pres_attrs = std::to_array<const SVGAnimatedPropertyBase*>(
      {x_.Get(), y_.Get(), width_.Get(), height_.Get()});
  AddAnimatedPropertiesToPresentationAttributeStyle(pres_attrs, style);
  SVGElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink
