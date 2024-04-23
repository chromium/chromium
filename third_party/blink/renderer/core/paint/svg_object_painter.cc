// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_object_painter.h"

#include "cc/paint/color_filter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"

namespace blink {

namespace {

bool ApplyPaintResource(
    const SvgContextPaints::ContextPaint& context_paint,
    const AffineTransform* additional_paint_server_transform,
    cc::PaintFlags& flags) {
  SVGElementResourceClient* client =
      SVGResources::GetClient(context_paint.object);
  if (!client) {
    return false;
  }
  auto* uri_resource = GetSVGResourceAsType<LayoutSVGResourcePaintServer>(
      *client, context_paint.paint.Resource());
  if (!uri_resource) {
    return false;
  }

  AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      context_paint.object.StyleRef(), DarkModeFilter::ElementRole::kSVG));
  if (!uri_resource->ApplyShader(
          *client, SVGResources::ReferenceBoxForEffects(context_paint.object),
          additional_paint_server_transform, auto_dark_mode, flags)) {
    return false;
  }
  return true;
}

void ApplyColorInterpolation(PaintFlags paint_flags,
                             const ComputedStyle& style,
                             cc::PaintFlags& flags) {
  const bool is_rendering_svg_mask = paint_flags & PaintFlag::kPaintingSVGMask;
  if (is_rendering_svg_mask &&
      style.ColorInterpolation() == EColorInterpolation::kLinearrgb) {
    flags.setColorFilter(cc::ColorFilter::MakeSRGBToLinearGamma());
  }
}

}  // namespace

bool SVGObjectPainter::HasVisibleStroke(
    const ComputedStyle& style,
    const SvgContextPaints* context_paints) {
  if (!style.HasVisibleStroke()) {
    return false;
  }
  switch (style.StrokePaint().type) {
    case SVGPaintType::kContextFill:
      return context_paints && !context_paints->fill.paint.IsNone();
    case SVGPaintType::kContextStroke:
      return context_paints && !context_paints->stroke.paint.IsNone();
    default:
      return true;
  }
}

bool SVGObjectPainter::HasFill(const ComputedStyle& style,
                               const SvgContextPaints* context_paints) {
  if (!style.HasFill()) {
    return false;
  }
  switch (style.FillPaint().type) {
    case SVGPaintType::kContextFill:
      return context_paints && !context_paints->fill.paint.IsNone();
    case SVGPaintType::kContextStroke:
      return context_paints && !context_paints->stroke.paint.IsNone();
    default:
      return true;
  }
}

void SVGObjectPainter::PaintResourceSubtree(GraphicsContext& context,
                                            PaintFlags additional_flags) {
  DCHECK(!layout_object_.SelfNeedsFullLayout());

  PaintInfo info(context, CullRect::Infinite(), PaintPhase::kForeground,
                 layout_object_.ChildPaintBlockedByDisplayLock(),
                 PaintFlag::kOmitCompositingInfo |
                     PaintFlag::kPaintingResourceSubtree | additional_flags);
  layout_object_.Paint(info);
}

SvgContextPaints::ContextPaint SVGObjectPainter::ResolveContextPaint(
    const SVGPaint& initial_paint) {
  switch (initial_paint.type) {
    case SVGPaintType::kContextFill:
      DCHECK(RuntimeEnabledFeatures::SvgContextPaintEnabled());
      return context_paints_
                 ? context_paints_->fill
                 : SvgContextPaints::ContextPaint(layout_object_, SVGPaint());
    case SVGPaintType::kContextStroke:
      DCHECK(RuntimeEnabledFeatures::SvgContextPaintEnabled());
      return context_paints_
                 ? context_paints_->stroke
                 : SvgContextPaints::ContextPaint(layout_object_, SVGPaint());
    default:
      return SvgContextPaints::ContextPaint(layout_object_, initial_paint);
  }
}

std::optional<AffineTransform> SVGObjectPainter::ResolveContextTransform(
    const SVGPaint& initial_paint,
    const AffineTransform* additional_paint_server_transform) {
  std::optional<AffineTransform> result;
  if (additional_paint_server_transform) {
    result.emplace(*additional_paint_server_transform);
  }
  switch (initial_paint.type) {
    case SVGPaintType::kContextFill:
    case SVGPaintType::kContextStroke:
      if (context_paints_) {
        result.emplace(result.value_or(AffineTransform()) *
                       context_paints_->transform.Inverse());
      }
      break;
    default:
      break;
  }
  return result;
}

bool SVGObjectPainter::PreparePaint(
    PaintFlags paint_flags,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode,
    cc::PaintFlags& flags,
    const AffineTransform* additional_paint_server_transform) {
  const bool apply_to_fill = resource_mode == kApplyToFillMode;
  const float alpha =
      apply_to_fill ? style.FillOpacity() : style.StrokeOpacity();
  const SVGPaint& initial_paint =
      apply_to_fill ? style.FillPaint() : style.StrokePaint();
  SvgContextPaints::ContextPaint context_paint(
      ResolveContextPaint(initial_paint));
  const SVGPaint& paint = context_paint.paint;
  DCHECK(paint.HasColor() || paint.HasUrl());

  if (paint.HasUrl()) {
    std::optional<AffineTransform> resolved_transform = ResolveContextTransform(
        initial_paint, additional_paint_server_transform);
    if (ApplyPaintResource(context_paint,
                           base::OptionalToPtr(resolved_transform), flags)) {
      flags.setColor(ScaleAlpha(SK_ColorBLACK, alpha));
      ApplyColorInterpolation(paint_flags, style, flags);
      return true;
    }
  }

  if (paint.HasColor()) {
    Color flag_color;
    if (initial_paint.type == SVGPaintType::kContextFill) {
      flag_color = style.VisitedDependentContextFill(
          paint, context_paint.object.StyleRef());
    } else if (initial_paint.type == SVGPaintType::kContextStroke) {
      flag_color = style.VisitedDependentContextStroke(
          paint, context_paint.object.StyleRef());
    } else {
      const Longhand& property =
          apply_to_fill ? static_cast<const Longhand&>(GetCSSPropertyFill())
                        : static_cast<const Longhand&>(GetCSSPropertyStroke());
      flag_color = style.VisitedDependentColor(property);
    }
    flag_color.SetAlpha(flag_color.Alpha() * alpha);
    flags.setColor(flag_color.toSkColor4f());
    flags.setShader(nullptr);
    ApplyColorInterpolation(paint_flags, style, flags);
    return true;
  }
  return false;
}

}  // namespace blink
