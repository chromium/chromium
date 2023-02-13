// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_object_painter.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/skia/include/core/SkColorFilter.h"

namespace blink {

namespace {

void ApplyColorInterpolation(const ComputedStyle& style,
                             cc::PaintFlags& flags) {
  if (style.ColorInterpolation() == EColorInterpolation::kLinearrgb) {
    flags.setColorFilter(SkColorFilters::SRGBToLinearGamma());
  }
}

}  // namespace

void SVGObjectPainter::PaintResourceSubtree(GraphicsContext& context) {
  DCHECK(!layout_object_.SelfNeedsLayout());

  PaintInfo info(
      context, CullRect::Infinite(), PaintPhase::kForeground,
      PaintFlag::kOmitCompositingInfo | PaintFlag::kPaintingResourceSubtree);
  layout_object_.Paint(info);
}

bool SVGObjectPainter::ApplyPaintResource(
    const SVGPaint& paint,
    const AffineTransform* additional_paint_server_transform,
    cc::PaintFlags& flags) {
  SVGElementResourceClient* client = SVGResources::GetClient(layout_object_);
  if (!client)
    return false;
  auto* uri_resource = GetSVGResourceAsType<LayoutSVGResourcePaintServer>(
      *client, paint.Resource());
  if (!uri_resource)
    return false;

  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      layout_object_.StyleRef(), DarkModeFilter::ElementRole::kSVG));
  if (!uri_resource->ApplyShader(
          *client, SVGResources::ReferenceBoxForEffects(layout_object_),
          additional_paint_server_transform, auto_dark_mode, flags))
    return false;
  return true;
}

bool SVGObjectPainter::PreparePaint(
    bool is_rendering_clip_path_as_mask_image,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode,
    cc::PaintFlags& flags,
    const AffineTransform* additional_paint_server_transform) {
  if (is_rendering_clip_path_as_mask_image) {
    if (resource_mode == kApplyToStrokeMode)
      return false;
    flags.setColor(SK_ColorBLACK);
    flags.setShader(nullptr);
    return true;
  }

  const bool apply_to_fill = resource_mode == kApplyToFillMode;
  const SVGPaint& paint =
      apply_to_fill ? style.FillPaint() : style.StrokePaint();
  const float alpha =
      apply_to_fill ? style.FillOpacity() : style.StrokeOpacity();
  if (paint.HasUrl()) {
    if (ApplyPaintResource(paint, additional_paint_server_transform, flags)) {
      flags.setColor(ScaleAlpha(SK_ColorBLACK, alpha));
      ApplyColorInterpolation(style, flags);
      return true;
    }
  }
  if (paint.HasColor()) {
    const Longhand& property = apply_to_fill
                                   ? To<Longhand>(GetCSSPropertyFill())
                                   : To<Longhand>(GetCSSPropertyStroke());
    const Color color = style.VisitedDependentColor(property);
    flags.setColor(ScaleAlpha(color.Rgb(), alpha));
    flags.setShader(nullptr);
    ApplyColorInterpolation(style, flags);
    return true;
  }
  return false;
}

}  // namespace blink
