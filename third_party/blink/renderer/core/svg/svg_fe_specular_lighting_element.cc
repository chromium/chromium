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

#include "third_party/blink/renderer/core/svg/svg_fe_specular_lighting_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_specular_lighting.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGFESpecularLightingElement::SVGFESpecularLightingElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFESpecularLightingTag,
                                           document),
      specular_constant_(MakeGarbageCollected<SVGAnimatedNumber>(
          this,
          svg_names::kSpecularConstantAttr,
          1)),
      specular_exponent_(MakeGarbageCollected<SVGAnimatedNumber>(
          this,
          svg_names::kSpecularExponentAttr,
          1)),
      surface_scale_(
          MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                  svg_names::kSurfaceScaleAttr,
                                                  1)),
      kernel_unit_length_(MakeGarbageCollected<SVGAnimatedNumberOptionalNumber>(
          this,
          svg_names::kKernelUnitLengthAttr,
          0.0f)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)) {
  AddToPropertyMap(specular_constant_);
  AddToPropertyMap(specular_exponent_);
  AddToPropertyMap(surface_scale_);
  AddToPropertyMap(kernel_unit_length_);
  AddToPropertyMap(in1_);
}

void SVGFESpecularLightingElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(specular_constant_);
  visitor->Trace(specular_exponent_);
  visitor->Trace(surface_scale_);
  visitor->Trace(kernel_unit_length_);
  visitor->Trace(in1_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

bool SVGFESpecularLightingElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FESpecularLighting* specular_lighting =
      static_cast<FESpecularLighting*>(effect);

  if (attr_name == svg_names::kLightingColorAttr) {
    const ComputedStyle& style = ComputedStyleRef();
    return specular_lighting->SetLightingColor(
        style.VisitedDependentColor(GetCSSPropertyLightingColor()));
  }
  if (attr_name == svg_names::kSurfaceScaleAttr)
    return specular_lighting->SetSurfaceScale(
        surface_scale_->CurrentValue()->Value());
  if (attr_name == svg_names::kSpecularConstantAttr)
    return specular_lighting->SetSpecularConstant(
        specular_constant_->CurrentValue()->Value());
  if (attr_name == svg_names::kSpecularExponentAttr)
    return specular_lighting->SetSpecularExponent(
        specular_exponent_->CurrentValue()->Value());

  LightSource* light_source =
      const_cast<LightSource*>(specular_lighting->GetLightSource());
  SVGFELightElement* light_element = SVGFELightElement::FindLightElement(*this);
  DCHECK(light_source);
  DCHECK(light_element);
  DCHECK(effect->GetFilter());

  if (attr_name == svg_names::kAzimuthAttr)
    return light_source->SetAzimuth(
        light_element->azimuth()->CurrentValue()->Value());
  if (attr_name == svg_names::kElevationAttr)
    return light_source->SetElevation(
        light_element->elevation()->CurrentValue()->Value());
  if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kZAttr)
    return light_source->SetPosition(
        effect->GetFilter()->Resolve3dPoint(light_element->GetPosition()));
  if (attr_name == svg_names::kPointsAtXAttr ||
      attr_name == svg_names::kPointsAtYAttr ||
      attr_name == svg_names::kPointsAtZAttr)
    return light_source->SetPointsAt(
        effect->GetFilter()->Resolve3dPoint(light_element->PointsAt()));
  if (attr_name == svg_names::kSpecularExponentAttr)
    return light_source->SetSpecularExponent(
        light_element->specularExponent()->CurrentValue()->Value());
  if (attr_name == svg_names::kLimitingConeAngleAttr)
    return light_source->SetLimitingConeAngle(
        light_element->limitingConeAngle()->CurrentValue()->Value());

  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFESpecularLightingElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kSurfaceScaleAttr ||
      attr_name == svg_names::kSpecularConstantAttr ||
      attr_name == svg_names::kSpecularExponentAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == svg_names::kInAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

void SVGFESpecularLightingElement::LightElementAttributeChanged(
    const SVGFELightElement* light_element,
    const QualifiedName& attr_name) {
  if (SVGFELightElement::FindLightElement(*this) != light_element)
    return;

  // The light element has different attribute names so attrName can identify
  // the requested attribute.
  PrimitiveAttributeChanged(attr_name);
}

FilterEffect* SVGFESpecularLightingElement::Build(
    SVGFilterBuilder* filter_builder,
    Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  const ComputedStyle* style = GetComputedStyle();
  if (!style)
    return nullptr;

  Color color = style->VisitedDependentColor(GetCSSPropertyLightingColor());

  const SVGFELightElement* light_node =
      SVGFELightElement::FindLightElement(*this);
  scoped_refptr<LightSource> light_source =
      light_node ? light_node->GetLightSource(filter) : nullptr;

  auto* effect = MakeGarbageCollected<FESpecularLighting>(
      filter, color, surface_scale_->CurrentValue()->Value(),
      specular_constant_->CurrentValue()->Value(),
      specular_exponent_->CurrentValue()->Value(), std::move(light_source));
  effect->InputEffects().push_back(input1);
  return effect;
}

bool SVGFESpecularLightingElement::TaintsOrigin() const {
  const ComputedStyle* style = GetComputedStyle();
  // TaintsOrigin() is only called after a successful call to Build()
  // (see above), so we should have a ComputedStyle here.
  DCHECK(style);
  return style->SvgStyle().LightingColor().IsCurrentColor();
}

}  // namespace blink
