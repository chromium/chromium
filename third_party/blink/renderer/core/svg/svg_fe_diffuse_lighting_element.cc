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

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number_optional_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg/svg_fe_light_element.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_diffuse_lighting.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/light_source.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

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
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)) {}

SVGAnimatedNumber* SVGFEDiffuseLightingElement::kernelUnitLengthX() {
  return kernel_unit_length_->FirstNumber();
}

SVGAnimatedNumber* SVGFEDiffuseLightingElement::kernelUnitLengthY() {
  return kernel_unit_length_->SecondNumber();
}

void SVGFEDiffuseLightingElement::Trace(Visitor* visitor) const {
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

  if (const auto* light_element = SVGFELightElement::FindLightElement(*this)) {
    std::optional<bool> light_source_update =
        light_element->SetLightSourceAttribute(diffuse_lighting, attr_name);
    if (light_source_update)
      return *light_source_update;
  }
  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFEDiffuseLightingElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kSurfaceScaleAttr ||
      attr_name == svg_names::kDiffuseConstantAttr ||
      attr_name == svg_names::kLightingColorAttr) {
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == svg_names::kInAttr) {
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(params);
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
  return style->LightingColor().IsCurrentColor();
}

SVGAnimatedPropertyBase* SVGFEDiffuseLightingElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kDiffuseConstantAttr) {
    return diffuse_constant_.Get();
  } else if (attribute_name == svg_names::kSurfaceScaleAttr) {
    return surface_scale_.Get();
  } else if (attribute_name == svg_names::kKernelUnitLengthAttr) {
    return kernel_unit_length_.Get();
  } else if (attribute_name == svg_names::kInAttr) {
    return in1_.Get();
  } else {
    return SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
        attribute_name);
  }
}

void SVGFEDiffuseLightingElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{diffuse_constant_.Get(),
                                   surface_scale_.Get(),
                                   kernel_unit_length_.Get(), in1_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes();
}

}  // namespace blink
