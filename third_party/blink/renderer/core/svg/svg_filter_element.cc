/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_filter_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGFilterElement::SVGFilterElement(Document& document)
    : SVGElement(svg_names::kFilterTag, document),
      SVGURIReference(this),
      // Spec: If the x/y attribute is not specified, the effect is as if a
      // value of "-10%" were specified.
      x_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kXAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercentMinus10)),
      y_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kYAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercentMinus10)),
      // Spec: If the width/height attribute is not specified, the effect is as
      // if a value of "120%" were specified.
      width_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kWidthAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercent120)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercent120)),
      filter_units_(MakeGarbageCollected<
                    SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kFilterUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)),
      primitive_units_(MakeGarbageCollected<
                       SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kPrimitiveUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {}

SVGFilterElement::~SVGFilterElement() = default;

void SVGFilterElement::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(filter_units_);
  visitor->Trace(primitive_units_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

void SVGFilterElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  bool is_xywh =
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr || attr_name == svg_names::kHeightAttr;
  if (is_xywh)
    UpdateRelativeLengthsInformation();

  if (is_xywh || attr_name == svg_names::kFilterUnitsAttr ||
      attr_name == svg_names::kPrimitiveUnitsAttr) {
    InvalidateFilterChain();
    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

LocalSVGResource* SVGFilterElement::AssociatedResource() const {
  return GetTreeScope().EnsureSVGTreeScopedResources().ExistingResourceForId(
      GetIdAttribute());
}

void SVGFilterElement::PrimitiveAttributeChanged(
    SVGFilterPrimitiveStandardAttributes& primitive,
    const QualifiedName& attribute) {
  if (LocalSVGResource* resource = AssociatedResource())
    resource->NotifyFilterPrimitiveChanged(primitive, attribute);
}

void SVGFilterElement::InvalidateFilterChain() {
  if (LocalSVGResource* resource = AssociatedResource())
    resource->NotifyContentChanged();
}

void SVGFilterElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (change.ByParser() && !AssociatedResource())
    return;

  InvalidateFilterChain();
}

LayoutObject* SVGFilterElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGResourceFilter>(this);
}

bool SVGFilterElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

SVGAnimatedPropertyBase* SVGFilterElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kWidthAttr) {
    return width_.Get();
  } else if (attribute_name == svg_names::kHeightAttr) {
    return height_.Get();
  } else if (attribute_name == svg_names::kFilterUnitsAttr) {
    return filter_units_.Get();
  } else if (attribute_name == svg_names::kPrimitiveUnitsAttr) {
    return primitive_units_.Get();
  } else {
    SVGAnimatedPropertyBase* ret =
        SVGURIReference::PropertyFromAttribute(attribute_name);
    if (ret) {
      return ret;
    } else {
      return SVGElement::PropertyFromAttribute(attribute_name);
    }
  }
}

void SVGFilterElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x_.Get(),
                                   y_.Get(),
                                   width_.Get(),
                                   height_.Get(),
                                   filter_units_.Get(),
                                   primitive_units_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
