/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_radial_gradient.h"

#include "third_party/blink/renderer/core/svg/svg_radial_gradient_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

LayoutSVGResourceRadialGradient::LayoutSVGResourceRadialGradient(
    SVGRadialGradientElement* node)
    : LayoutSVGResourceGradient(node),
      attributes_wrapper_(
          MakeGarbageCollected<RadialGradientAttributesWrapper>()) {}

LayoutSVGResourceRadialGradient::~LayoutSVGResourceRadialGradient() = default;

void LayoutSVGResourceRadialGradient::CollectGradientAttributes() {
  DCHECK(GetElement());
  attributes_wrapper_->Set(RadialGradientAttributes());
  To<SVGRadialGradientElement>(GetElement())
      ->CollectGradientAttributes(MutableAttributes());
}

FloatPoint LayoutSVGResourceRadialGradient::CenterPoint(
    const RadialGradientAttributes& attributes) const {
  return SVGLengthContext::ResolvePoint(GetElement(),
                                        attributes.GradientUnits(),
                                        *attributes.Cx(), *attributes.Cy());
}

FloatPoint LayoutSVGResourceRadialGradient::FocalPoint(
    const RadialGradientAttributes& attributes) const {
  return SVGLengthContext::ResolvePoint(GetElement(),
                                        attributes.GradientUnits(),
                                        *attributes.Fx(), *attributes.Fy());
}

float LayoutSVGResourceRadialGradient::Radius(
    const RadialGradientAttributes& attributes) const {
  return SVGLengthContext::ResolveLength(
      GetElement(), attributes.GradientUnits(), *attributes.R());
}

float LayoutSVGResourceRadialGradient::FocalRadius(
    const RadialGradientAttributes& attributes) const {
  return SVGLengthContext::ResolveLength(
      GetElement(), attributes.GradientUnits(), *attributes.Fr());
}

scoped_refptr<Gradient> LayoutSVGResourceRadialGradient::BuildGradient() const {
  const RadialGradientAttributes& attributes = Attributes();
  scoped_refptr<Gradient> gradient = Gradient::CreateRadial(
      FocalPoint(attributes), FocalRadius(attributes), CenterPoint(attributes),
      Radius(attributes), 1,
      PlatformSpreadMethodFromSVGType(attributes.SpreadMethod()),
      Gradient::ColorInterpolation::kUnpremultiplied,
      Gradient::DegenerateHandling::kAllow);
  gradient->AddColorStops(attributes.Stops());
  return gradient;
}

}  // namespace blink
