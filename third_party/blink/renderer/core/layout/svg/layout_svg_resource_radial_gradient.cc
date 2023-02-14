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

namespace blink {

LayoutSVGResourceRadialGradient::LayoutSVGResourceRadialGradient(
    SVGRadialGradientElement* node)
    : LayoutSVGResourceGradient(node) {}

LayoutSVGResourceRadialGradient::~LayoutSVGResourceRadialGradient() = default;

void LayoutSVGResourceRadialGradient::Trace(Visitor* visitor) const {
  visitor->Trace(attributes_);
  LayoutSVGResourceGradient::Trace(visitor);
}

const GradientAttributes& LayoutSVGResourceRadialGradient::EnsureAttributes()
    const {
  NOT_DESTROYED();
  DCHECK(GetElement());
  if (should_collect_gradient_attributes_) {
    attributes_ =
        To<SVGRadialGradientElement>(*GetElement()).CollectGradientAttributes();
    should_collect_gradient_attributes_ = false;
  }
  return attributes_;
}

gfx::PointF LayoutSVGResourceRadialGradient::CenterPoint(
    const RadialGradientAttributes& attributes) const {
  NOT_DESTROYED();
  return ResolvePoint(attributes.GradientUnits(), *attributes.Cx(),
                      *attributes.Cy());
}

gfx::PointF LayoutSVGResourceRadialGradient::FocalPoint(
    const RadialGradientAttributes& attributes) const {
  NOT_DESTROYED();
  return ResolvePoint(attributes.GradientUnits(), *attributes.Fx(),
                      *attributes.Fy());
}

float LayoutSVGResourceRadialGradient::Radius(
    const RadialGradientAttributes& attributes) const {
  NOT_DESTROYED();
  return ResolveRadius(attributes.GradientUnits(), *attributes.R());
}

float LayoutSVGResourceRadialGradient::FocalRadius(
    const RadialGradientAttributes& attributes) const {
  NOT_DESTROYED();
  return ResolveRadius(attributes.GradientUnits(), *attributes.Fr());
}

scoped_refptr<Gradient> LayoutSVGResourceRadialGradient::BuildGradient() const {
  NOT_DESTROYED();
  DCHECK(!should_collect_gradient_attributes_);
  return Gradient::CreateRadial(
      FocalPoint(attributes_), FocalRadius(attributes_),
      CenterPoint(attributes_), Radius(attributes_), 1,
      PlatformSpreadMethodFromSVGType(attributes_.SpreadMethod()),
      Gradient::ColorInterpolation::kUnpremultiplied,
      Gradient::DegenerateHandling::kAllow);
}

}  // namespace blink
