/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"

#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace blink {

namespace {

// If |SVGPaintDescription::has_fallback| is true, |SVGPaintDescription::color|
// is set to a fallback color.
struct SVGPaintDescription {
  STACK_ALLOCATED();

 public:
  SVGPaintDescription() = default;
  explicit SVGPaintDescription(Color color) : color(color), is_valid(true) {}
  explicit SVGPaintDescription(LayoutSVGResourcePaintServer* resource)
      : resource(resource), is_valid(true) {
    DCHECK(resource);
  }
  SVGPaintDescription(LayoutSVGResourcePaintServer* resource,
                      Color fallback_color)
      : resource(resource),
        color(fallback_color),
        is_valid(true),
        has_fallback(true) {
    DCHECK(resource);
  }

  LayoutSVGResourcePaintServer* resource = nullptr;
  Color color;
  bool is_valid = false;
  bool has_fallback = false;
};

}  // namespace

SVGPaintServer::SVGPaintServer(Color color) : color_(color) {}

SVGPaintServer::SVGPaintServer(scoped_refptr<Gradient> gradient,
                               const AffineTransform& transform)
    : gradient_(std::move(gradient)),
      transform_(transform),
      color_(Color::kBlack) {}

SVGPaintServer::SVGPaintServer(scoped_refptr<Pattern> pattern,
                               const AffineTransform& transform)
    : pattern_(std::move(pattern)),
      transform_(transform),
      color_(Color::kBlack) {}

void SVGPaintServer::ApplyToPaintFlags(PaintFlags& flags, float alpha) {
  SkColor base_color = gradient_ || pattern_ ? SK_ColorBLACK : color_.Rgb();
  flags.setColor(ScaleAlpha(base_color, alpha));
  if (pattern_) {
    pattern_->ApplyToFlags(flags, AffineTransformToSkMatrix(transform_));
  } else if (gradient_) {
    gradient_->ApplyToFlags(flags, AffineTransformToSkMatrix(transform_));
  } else {
    flags.setShader(nullptr);
  }
}

void SVGPaintServer::PrependTransform(const AffineTransform& transform) {
  DCHECK(gradient_ || pattern_);
  transform_ = transform * transform_;
}

static base::Optional<Color> ResolveColor(const ComputedStyle& style,
                                          const SVGPaint& paint,
                                          const SVGPaint& visited_paint) {
  if (!paint.HasColor())
    return base::nullopt;
  Color color = style.ResolvedColor(paint.GetColor());
  if (style.InsideLink() != EInsideLink::kInsideVisitedLink)
    return color;
  // FIXME: This code doesn't support the uri component of the visited link
  // paint, https://bugs.webkit.org/show_bug.cgi?id=70006
  if (!visited_paint.HasColor())
    return color;
  const Color& visited_color = style.ResolvedColor(visited_paint.GetColor());
  return Color(visited_color.Red(), visited_color.Green(), visited_color.Blue(),
               color.Alpha());
}

static SVGPaintDescription RequestPaint(const LayoutObject& object,
                                        const ComputedStyle& style,
                                        LayoutSVGResourceMode mode) {
  bool apply_to_fill = mode == kApplyToFillMode;

  const SVGComputedStyle& svg_style = style.SvgStyle();
  const SVGPaint& paint =
      apply_to_fill ? svg_style.FillPaint() : svg_style.StrokePaint();
  const SVGPaint& visited_paint = apply_to_fill
                                      ? svg_style.InternalVisitedFillPaint()
                                      : svg_style.InternalVisitedStrokePaint();
  base::Optional<Color> color = ResolveColor(style, paint, visited_paint);

  if (paint.HasUrl()) {
    LayoutSVGResourcePaintServer* uri_resource = nullptr;
    if (SVGResources* resources =
            SVGResourcesCache::CachedResourcesForLayoutObject(object))
      uri_resource = apply_to_fill ? resources->Fill() : resources->Stroke();
    if (uri_resource) {
      // The paint server resource exists, though it may be invalid (pattern
      // with width/height=0). Return the fallback color to our caller so it can
      // use it, if PreparePaintServer() on the resource container failed.
      if (color)
        return SVGPaintDescription(uri_resource, *color);
      return SVGPaintDescription(uri_resource);
    }
    // If the requested resource is not available, return the color resource or
    // 'none'.
  }

  // Color or fallback color.
  if (color)
    return SVGPaintDescription(*color);

  // Either 'none' or a 'none' fallback. (SVG2 say 'none' is implied when no
  // fallback is specified.)
  return SVGPaintDescription();
}

SVGPaintServer SVGPaintServer::RequestForLayoutObject(
    const LayoutObject& layout_object,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode) {
  DCHECK(resource_mode == kApplyToFillMode ||
         resource_mode == kApplyToStrokeMode);

  SVGPaintDescription paint_description =
      RequestPaint(layout_object, style, resource_mode);
  if (!paint_description.is_valid)
    return Invalid();
  if (!paint_description.resource)
    return SVGPaintServer(paint_description.color);
  SVGPaintServer paint_server = paint_description.resource->PreparePaintServer(
      *SVGResources::GetClient(layout_object),
      SVGResources::ReferenceBoxForEffects(layout_object));
  if (paint_server.IsValid())
    return paint_server;
  if (paint_description.has_fallback)
    return SVGPaintServer(paint_description.color);
  return Invalid();
}

bool SVGPaintServer::ExistsForLayoutObject(
    const LayoutObject& layout_object,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode) {
  return RequestPaint(layout_object, style, resource_mode).is_valid;
}

LayoutSVGResourcePaintServer::LayoutSVGResourcePaintServer(SVGElement* element)
    : LayoutSVGResourceContainer(element) {}

LayoutSVGResourcePaintServer::~LayoutSVGResourcePaintServer() = default;

}  // namespace blink
