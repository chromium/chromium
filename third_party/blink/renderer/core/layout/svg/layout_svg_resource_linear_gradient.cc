/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_linear_gradient.h"

#include "third_party/blink/renderer/core/svg/svg_linear_gradient_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

LayoutSVGResourceLinearGradient::LayoutSVGResourceLinearGradient(
    SVGLinearGradientElement* node)
    : LayoutSVGResourceGradient(node),
      attributes_wrapper_(
          MakeGarbageCollected<LinearGradientAttributesWrapper>()) {}

LayoutSVGResourceLinearGradient::~LayoutSVGResourceLinearGradient() = default;

void LayoutSVGResourceLinearGradient::CollectGradientAttributes() {
  DCHECK(GetElement());
  attributes_wrapper_->Set(LinearGradientAttributes());
  To<SVGLinearGradientElement>(GetElement())
      ->CollectGradientAttributes(MutableAttributes());
}

FloatPoint LayoutSVGResourceLinearGradient::StartPoint(
    const LinearGradientAttributes& attributes) const {
  return SVGLengthContext::ResolvePoint(GetElement(),
                                        attributes.GradientUnits(),
                                        *attributes.X1(), *attributes.Y1());
}

FloatPoint LayoutSVGResourceLinearGradient::EndPoint(
    const LinearGradientAttributes& attributes) const {
  return SVGLengthContext::ResolvePoint(GetElement(),
                                        attributes.GradientUnits(),
                                        *attributes.X2(), *attributes.Y2());
}

scoped_refptr<Gradient> LayoutSVGResourceLinearGradient::BuildGradient() const {
  const LinearGradientAttributes& attributes = Attributes();
  scoped_refptr<Gradient> gradient = Gradient::CreateLinear(
      StartPoint(attributes), EndPoint(attributes),
      PlatformSpreadMethodFromSVGType(attributes.SpreadMethod()),
      Gradient::ColorInterpolation::kUnpremultiplied,
      Gradient::DegenerateHandling::kAllow);
  gradient->AddColorStops(attributes.Stops());
  return gradient;
}

}  // namespace blink
