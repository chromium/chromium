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
#include "third_party/blink/renderer/core/svg/svg_animated_number.h"
#include "third_party/blink/renderer/core/svg/svg_fe_diffuse_lighting_element.h"
#include "third_party/blink/renderer/core/svg/svg_fe_specular_lighting_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_lighting.h"
#include "third_party/blink/renderer/platform/graphics/filters/light_source.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/point3_f.h"

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
          0.0f)) {}

void SVGFELightElement::Trace(Visitor* visitor) const {
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

gfx::Point3F SVGFELightElement::GetPosition() const {
  return gfx::Point3F(x()->CurrentValue()->Value(),
                      y()->CurrentValue()->Value(),
                      z()->CurrentValue()->Value());
}

gfx::Point3F SVGFELightElement::PointsAt() const {
  return gfx::Point3F(pointsAtX()->CurrentValue()->Value(),
                      pointsAtY()->CurrentValue()->Value(),
                      pointsAtZ()->CurrentValue()->Value());
}

std::optional<bool> SVGFELightElement::SetLightSourceAttribute(
    FELighting* lighting_effect,
    const QualifiedName& attr_name) const {
  LightSource* light_source = lighting_effect->GetLightSource();
  DCHECK(light_source);

  const Filter* filter = lighting_effect->GetFilter();
  DCHECK(filter);
  if (attr_name == svg_names::kAzimuthAttr)
    return light_source->SetAzimuth(azimuth()->CurrentValue()->Value());
  if (attr_name == svg_names::kElevationAttr)
    return light_source->SetElevation(elevation()->CurrentValue()->Value());
  if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kZAttr)
    return light_source->SetPosition(filter->Resolve3dPoint(GetPosition()));
  if (attr_name == svg_names::kPointsAtXAttr ||
      attr_name == svg_names::kPointsAtYAttr ||
      attr_name == svg_names::kPointsAtZAttr)
    return light_source->SetPointsAt(filter->Resolve3dPoint(PointsAt()));
  if (attr_name == svg_names::kSpecularExponentAttr) {
    return light_source->SetSpecularExponent(
        specularExponent()->CurrentValue()->Value());
  }
  if (attr_name == svg_names::kLimitingConeAngleAttr) {
    return light_source->SetLimitingConeAngle(
        limitingConeAngle()->CurrentValue()->Value());
  }
  return std::nullopt;
}

void SVGFELightElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
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
    if (!layout_object || !layout_object->IsSVGFilterPrimitive())
      return;

    if (auto* diffuse = DynamicTo<SVGFEDiffuseLightingElement>(*parent))
      diffuse->LightElementAttributeChanged(this, attr_name);
    else if (auto* specular = DynamicTo<SVGFESpecularLightingElement>(*parent))
      specular->LightElementAttributeChanged(this, attr_name);

    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

void SVGFELightElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (!change.ByParser()) {
    if (ContainerNode* parent = parentNode()) {
      LayoutObject* layout_object = parent->GetLayoutObject();
      if (layout_object && layout_object->IsSVGFilterPrimitive())
        MarkForLayoutAndParentResourceInvalidation(*layout_object);
    }
  }
}

SVGAnimatedPropertyBase* SVGFELightElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kAzimuthAttr) {
    return azimuth_.Get();
  } else if (attribute_name == svg_names::kElevationAttr) {
    return elevation_.Get();
  } else if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kZAttr) {
    return z_.Get();
  } else if (attribute_name == svg_names::kPointsAtXAttr) {
    return points_at_x_.Get();
  } else if (attribute_name == svg_names::kPointsAtYAttr) {
    return points_at_y_.Get();
  } else if (attribute_name == svg_names::kPointsAtZAttr) {
    return points_at_z_.Get();
  } else if (attribute_name == svg_names::kSpecularExponentAttr) {
    return specular_exponent_.Get();
  } else if (attribute_name == svg_names::kLimitingConeAngleAttr) {
    return limiting_cone_angle_.Get();
  } else {
    return SVGElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGFELightElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{azimuth_.Get(),
                                   elevation_.Get(),
                                   x_.Get(),
                                   y_.Get(),
                                   z_.Get(),
                                   points_at_x_.Get(),
                                   points_at_y_.Get(),
                                   points_at_z_.Get(),
                                   specular_exponent_.Get(),
                                   limiting_cone_angle_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
