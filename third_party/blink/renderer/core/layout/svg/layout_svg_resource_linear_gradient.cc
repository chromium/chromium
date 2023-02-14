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

namespace blink {

LayoutSVGResourceLinearGradient::LayoutSVGResourceLinearGradient(
    SVGLinearGradientElement* node)
    : LayoutSVGResourceGradient(node) {}

LayoutSVGResourceLinearGradient::~LayoutSVGResourceLinearGradient() = default;

void LayoutSVGResourceLinearGradient::Trace(Visitor* visitor) const {
  visitor->Trace(attributes_);
  LayoutSVGResourceGradient::Trace(visitor);
}

const GradientAttributes& LayoutSVGResourceLinearGradient::EnsureAttributes()
    const {
  NOT_DESTROYED();
  DCHECK(GetElement());
  if (should_collect_gradient_attributes_) {
    attributes_ =
        To<SVGLinearGradientElement>(*GetElement()).CollectGradientAttributes();
    should_collect_gradient_attributes_ = false;
  }
  return attributes_;
}

gfx::PointF LayoutSVGResourceLinearGradient::StartPoint(
    const LinearGradientAttributes& attributes) const {
  NOT_DESTROYED();
  return ResolvePoint(attributes.GradientUnits(), *attributes.X1(),
                      *attributes.Y1());
}

gfx::PointF LayoutSVGResourceLinearGradient::EndPoint(
    const LinearGradientAttributes& attributes) const {
  NOT_DESTROYED();
  return ResolvePoint(attributes.GradientUnits(), *attributes.X2(),
                      *attributes.Y2());
}

scoped_refptr<Gradient> LayoutSVGResourceLinearGradient::BuildGradient() const {
  NOT_DESTROYED();
  DCHECK(!should_collect_gradient_attributes_);
  return Gradient::CreateLinear(
      StartPoint(attributes_), EndPoint(attributes_),
      PlatformSpreadMethodFromSVGType(attributes_.SpreadMethod()),
      Gradient::ColorInterpolation::kUnpremultiplied,
      Gradient::DegenerateHandling::kAllow);
}

}  // namespace blink
