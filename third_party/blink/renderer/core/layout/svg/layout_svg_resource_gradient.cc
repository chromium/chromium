/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_gradient.h"

#include <memory>

#include "third_party/blink/renderer/core/svg/gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/platform/graphics/gradient.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

namespace {

gfx::SizeF MakeViewport(const SVGViewportResolver& viewport_resolver,
                        const LengthPoint& point,
                        SVGUnitTypes::SVGUnitType type) {
  if (!point.X().HasPercent() && !point.Y().HasPercent()) {
    return gfx::SizeF(0, 0);
  }
  if (type == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    return gfx::SizeF(1, 1);
  }
  return viewport_resolver.ResolveViewport();
}

float MakeViewportDimension(const SVGViewportResolver& viewport_resolver,
                            const Length& radius,
                            SVGUnitTypes::SVGUnitType type) {
  if (!radius.HasPercent()) {
    return 0;
  }
  if (type == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    return 1;
  }
  return viewport_resolver.ViewportDimension(SVGLengthMode::kOther);
}

}  // unnamed namespace

struct GradientData {
  USING_FAST_MALLOC(GradientData);

 public:
  scoped_refptr<Gradient> gradient;
  AffineTransform userspace_transform;
};

LayoutSVGResourceGradient::LayoutSVGResourceGradient(SVGGradientElement* node)
    : LayoutSVGResourcePaintServer(node) {}

void LayoutSVGResourceGradient::Trace(Visitor* visitor) const {
  visitor->Trace(gradient_map_);
  LayoutSVGResourcePaintServer::Trace(visitor);
}

void LayoutSVGResourceGradient::RemoveAllClientsFromCache() {
  NOT_DESTROYED();
  gradient_map_.clear();
  should_collect_gradient_attributes_ = true;
  To<SVGGradientElement>(*GetElement()).InvalidateDependentGradients();
  MarkAllClientsForInvalidation(kPaintInvalidation);
}

bool LayoutSVGResourceGradient::RemoveClientFromCache(
    SVGResourceClient& client) {
  NOT_DESTROYED();
  auto entry = gradient_map_.find(&client);
  if (entry == gradient_map_.end()) {
    return false;
  }
  gradient_map_.erase(entry);
  return true;
}

std::unique_ptr<GradientData> LayoutSVGResourceGradient::BuildGradientData(
    const gfx::RectF& object_bounding_box) const {
  NOT_DESTROYED();
  // Create gradient object
  auto gradient_data = std::make_unique<GradientData>();

  // Validate gradient DOM state before building the actual
  // gradient. This should avoid tearing down the gradient we're
  // currently working on. Preferably the state validation should have
  // no side-effects though.
  const GradientAttributes& attributes = EnsureAttributes();

  // We want the text bounding box applied to the gradient space transform
  // now, so the gradient shader can use it.
  if (attributes.GradientUnits() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    // Spec: When the geometry of the applicable element has no width or height
    // and objectBoundingBox is specified, then the given effect (e.g. a
    // gradient or a filter) will be ignored.
    if (object_bounding_box.IsEmpty())
      return gradient_data;
    gradient_data->userspace_transform.Translate(object_bounding_box.x(),
                                                 object_bounding_box.y());
    gradient_data->userspace_transform.ScaleNonUniform(
        object_bounding_box.width(), object_bounding_box.height());
  }

  if (!attributes.GradientTransform().IsInvertible()) {
    return gradient_data;
  }

  // Create gradient object
  gradient_data->gradient = BuildGradient();
  if (RuntimeEnabledFeatures::
          SvgGradientColorInterpolationLinearRgbSupportEnabled()) {
    gradient_data->gradient->SetColorInterpolationSpace(
        StyleRef().ColorInterpolation() == EColorInterpolation::kLinearrgb
            ? Color::ColorSpace::kSRGBLinear
            : Color::ColorSpace::kNone,
        Color::HueInterpolationMethod::kShorter);
  }
  gradient_data->gradient->AddColorStops(attributes.Stops());

  gradient_data->userspace_transform *= attributes.GradientTransform();

  return gradient_data;
}

bool LayoutSVGResourceGradient::ApplyShader(
    const SVGResourceClient& client,
    const gfx::RectF& reference_box,
    const AffineTransform* additional_transform,
    const AutoDarkMode& auto_dark_mode,
    cc::PaintFlags& flags) {
  NOT_DESTROYED();
  ClearInvalidationMask();

  std::unique_ptr<GradientData>& gradient_data =
      gradient_map_.insert(&client, nullptr).stored_value->value;
  if (!gradient_data)
    gradient_data = BuildGradientData(reference_box);

  if (!gradient_data->gradient)
    return false;

  AffineTransform transform = gradient_data->userspace_transform;
  if (additional_transform)
    transform = *additional_transform * transform;
  ImageDrawOptions draw_options;
  draw_options.apply_dark_mode =
      auto_dark_mode.enabled && StyleRef().ForceDark();
  gradient_data->gradient->ApplyToFlags(
      flags, AffineTransformToSkMatrix(transform), draw_options);
  return true;
}

bool LayoutSVGResourceGradient::IsChildAllowed(LayoutObject* child,
                                               const ComputedStyle&) const {
  NOT_DESTROYED();
  if (!child->IsSVGResourceContainer())
    return false;

  return To<LayoutSVGResourceContainer>(child)->IsSVGPaintServer();
}

gfx::PointF LayoutSVGResourceGradient::ResolvePoint(
    SVGUnitTypes::SVGUnitType type,
    const SVGLength& x,
    const SVGLength& y) const {
  NOT_DESTROYED();
  const SVGViewportResolver viewport_resolver(*this);
  const SVGLengthConversionData conversion_data(*this);
  const LengthPoint point(x.ConvertToLength(conversion_data),
                          y.ConvertToLength(conversion_data));
  return PointForLengthPoint(point,
                             MakeViewport(viewport_resolver, point, type));
}

float LayoutSVGResourceGradient::ResolveRadius(SVGUnitTypes::SVGUnitType type,
                                               const SVGLength& r) const {
  NOT_DESTROYED();
  const SVGViewportResolver viewport_resolver(*this);
  const SVGLengthConversionData conversion_data(*this);
  const Length& radius = r.ConvertToLength(conversion_data);
  return FloatValueForLength(
      radius, MakeViewportDimension(viewport_resolver, radius, type));
}

GradientSpreadMethod LayoutSVGResourceGradient::PlatformSpreadMethodFromSVGType(
    SVGSpreadMethodType method) {
  switch (method) {
    case kSVGSpreadMethodUnknown:
    case kSVGSpreadMethodPad:
      return kSpreadMethodPad;
    case kSVGSpreadMethodReflect:
      return kSpreadMethodReflect;
    case kSVGSpreadMethodRepeat:
      return kSpreadMethodRepeat;
  }

  NOTREACHED_IN_MIGRATION();
  return kSpreadMethodPad;
}

}  // namespace blink
