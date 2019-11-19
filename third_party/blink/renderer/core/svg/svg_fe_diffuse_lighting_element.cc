/*
 * Copyright (C) 2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>
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

#include "third_party/blink/renderer/core/svg/svg_fe_diffuse_lighting_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_diffuse_lighting.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGFEDiffuseLightingElement::SVGFEDiffuseLightingElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEDiffuseLightingTag,
                                           document),
      diffuse_constant_(MakeGarbageCollected<SVGAnimatedNumber>(
          this,
          svg_names::kDiffuseConstantAttr,
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
  AddToPropertyMap(diffuse_constant_);
  AddToPropertyMap(surface_scale_);
  AddToPropertyMap(kernel_unit_length_);
  AddToPropertyMap(in1_);
}

void SVGFEDiffuseLightingElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(diffuse_constant_);
  visitor->Trace(surface_scale_);
  visitor->Trace(kernel_unit_length_);
  visitor->Trace(in1_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

bool SVGFEDiffuseLightingElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEDiffuseLighting* diffuse_lighting = static_cast<FEDiffuseLighting*>(effect);

  if (attr_name == svg_names::kLightingColorAttr) {
    const ComputedStyle& style = ComputedStyleRef();
    return diffuse_lighting->SetLightingColor(
        style.VisitedDependentColor(GetCSSPropertyLightingColor()));
  }
  if (attr_name == svg_names::kSurfaceScaleAttr)
    return diffuse_lighting->SetSurfaceScale(
        surface_scale_->CurrentValue()->Value());
  if (attr_name == svg_names::kDiffuseConstantAttr)
    return diffuse_lighting->SetDiffuseConstant(
        diffuse_constant_->CurrentValue()->Value());

  LightSource* light_source =
      const_cast<LightSource*>(diffuse_lighting->GetLightSource());
  const SVGFELightElement* light_element =
      SVGFELightElement::FindLightElement(*this);
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

void SVGFEDiffuseLightingElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kSurfaceScaleAttr ||
      attr_name == svg_names::kDiffuseConstantAttr ||
      attr_name == svg_names::kLightingColorAttr) {
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

void SVGFEDiffuseLightingElement::LightElementAttributeChanged(
    const SVGFELightElement* light_element,
    const QualifiedName& attr_name) {
  if (SVGFELightElement::FindLightElement(*this) != light_element)
    return;

  // The light element has different attribute names.
  PrimitiveAttributeChanged(attr_name);
}

FilterEffect* SVGFEDiffuseLightingElement::Build(
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

  auto* effect = MakeGarbageCollected<FEDiffuseLighting>(
      filter, color, surface_scale_->CurrentValue()->Value(),
      diffuse_constant_->CurrentValue()->Value(), std::move(light_source));
  effect->InputEffects().push_back(input1);
  return effect;
}

bool SVGFEDiffuseLightingElement::TaintsOrigin() const {
  const ComputedStyle* style = GetComputedStyle();
  // TaintsOrigin() is only called after a successful call to Build()
  // (see above), so we should have a ComputedStyle here.
  DCHECK(style);
  return style->SvgStyle().LightingColor().IsCurrentColor();
}

}  // namespace blink
