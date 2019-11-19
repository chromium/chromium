/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
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

#include "third_party/blink/renderer/core/svg/svg_fe_light_element.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/svg_fe_diffuse_lighting_element.h"
#include "third_party/blink/renderer/core/svg/svg_fe_specular_lighting_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGFELightElement::SVGFELightElement(const QualifiedName& tag_name,
                                     Document& document)
    : SVGElement(tag_name, document),
      azimuth_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                       svg_names::kAzimuthAttr,
                                                       0.0f)),
      elevation_(
          MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kElevationAttr,
                                                  0.0f)),
      x_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                 svg_names::kXAttr,
                                                 0.0f)),
      y_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                 svg_names::kYAttr,
                                                 0.0f)),
      z_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                 svg_names::kZAttr,
                                                 0.0f)),
      points_at_x_(
          MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kPointsAtXAttr,
                                                  0.0f)),
      points_at_y_(
          MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kPointsAtYAttr,
                                                  0.0f)),
      points_at_z_(
          MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kPointsAtZAttr,
                                                  0.0f)),
      specular_exponent_(MakeGarbageCollected<SVGAnimatedNumber>(
          this,
          svg_names::kSpecularExponentAttr,
          1)),
      limiting_cone_angle_(MakeGarbageCollected<SVGAnimatedNumber>(
          this,
          svg_names::kLimitingConeAngleAttr,
          0.0f)) {
  AddToPropertyMap(azimuth_);
  AddToPropertyMap(elevation_);
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(z_);
  AddToPropertyMap(points_at_x_);
  AddToPropertyMap(points_at_y_);
  AddToPropertyMap(points_at_z_);
  AddToPropertyMap(specular_exponent_);
  AddToPropertyMap(limiting_cone_angle_);
}

void SVGFELightElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(azimuth_);
  visitor->Trace(elevation_);
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(z_);
  visitor->Trace(points_at_x_);
  visitor->Trace(points_at_y_);
  visitor->Trace(points_at_z_);
  visitor->Trace(specular_exponent_);
  visitor->Trace(limiting_cone_angle_);
  SVGElement::Trace(visitor);
}

SVGFELightElement* SVGFELightElement::FindLightElement(
    const SVGElement& svg_element) {
  return Traversal<SVGFELightElement>::FirstChild(svg_element);
}

FloatPoint3D SVGFELightElement::GetPosition() const {
  return FloatPoint3D(x()->CurrentValue()->Value(),
                      y()->CurrentValue()->Value(),
                      z()->CurrentValue()->Value());
}

FloatPoint3D SVGFELightElement::PointsAt() const {
  return FloatPoint3D(pointsAtX()->CurrentValue()->Value(),
                      pointsAtY()->CurrentValue()->Value(),
                      pointsAtZ()->CurrentValue()->Value());
}

void SVGFELightElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kAzimuthAttr ||
      attr_name == svg_names::kElevationAttr ||
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kZAttr ||
      attr_name == svg_names::kPointsAtXAttr ||
      attr_name == svg_names::kPointsAtYAttr ||
      attr_name == svg_names::kPointsAtZAttr ||
      attr_name == svg_names::kSpecularExponentAttr ||
      attr_name == svg_names::kLimitingConeAngleAttr) {
    ContainerNode* parent = parentNode();
    if (!parent)
      return;

    LayoutObject* layout_object = parent->GetLayoutObject();
    if (!layout_object || !layout_object->IsSVGResourceFilterPrimitive())
      return;

    SVGElement::InvalidationGuard invalidation_guard(this);
    if (auto* diffuse = ToSVGFEDiffuseLightingElementOrNull(*parent))
      diffuse->LightElementAttributeChanged(this, attr_name);
    else if (auto* specular = ToSVGFESpecularLightingElementOrNull(*parent))
      specular->LightElementAttributeChanged(this, attr_name);

    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

void SVGFELightElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (!change.by_parser) {
    if (ContainerNode* parent = parentNode()) {
      LayoutObject* layout_object = parent->GetLayoutObject();
      if (layout_object && layout_object->IsSVGResourceFilterPrimitive())
        MarkForLayoutAndParentResourceInvalidation(*layout_object);
    }
  }
}

}  // namespace blink
