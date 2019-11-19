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

  // If we have no, ignore it.
  if (paint.IsNone())
    return SVGPaintDescription();

  Color color = paint.GetColor();
  bool has_color = paint.HasColor();

  if (paint.HasCurrentColor())
    color = style.VisitedDependentColor(GetCSSPropertyColor());

  if (style.InsideLink() == EInsideLink::kInsideVisitedLink) {
    // FIXME: This code doesn't support the uri component of the visited link
    // paint, https://bugs.webkit.org/show_bug.cgi?id=70006

    // If the color (primary or fallback) is 'currentcolor', then |color|
    // already contains the 'visited color'.
    if (!visited_paint.HasUrl() && !visited_paint.HasCurrentColor()) {
      const Color& visited_color = visited_paint.GetColor();
      color = Color(visited_color.Red(), visited_color.Green(),
                    visited_color.Blue(), color.Alpha());
      has_color = true;
    }
  }

  // If the primary resource is just a color, return immediately.
  if (!paint.HasUrl()) {
    // |paint.type| will be either <current-color> or <rgb-color> here - both of
    // which will have a color.
    DCHECK(has_color);
    return SVGPaintDescription(color);
  }

  LayoutSVGResourcePaintServer* uri_resource = nullptr;
  if (SVGResources* resources =
          SVGResourcesCache::CachedResourcesForLayoutObject(object))
    uri_resource = apply_to_fill ? resources->Fill() : resources->Stroke();

  // If the requested resource is not available, return the color resource or
  // 'none'.
  if (!uri_resource) {
    // The fallback is 'none'. (SVG2 say 'none' is implied when no fallback is
    // specified.)
    if (!paint.HasFallbackColor() || !has_color)
      return SVGPaintDescription();

    return SVGPaintDescription(color);
  }

  // The paint server resource exists, though it may be invalid (pattern with
  // width/height=0). Return the fallback color to our caller so it can use it,
  // if preparePaintServer() on the resource container failed.
  if (has_color)
    return SVGPaintDescription(uri_resource, color);

  return SVGPaintDescription(uri_resource);
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
      layout_object.ObjectBoundingBox());
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

SVGPaintDescription LayoutSVGResourcePaintServer::RequestPaintDescription(
    const LayoutObject& layout_object,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode) {
  return RequestPaint(layout_object, style, resource_mode);
}

}  // namespace blink
