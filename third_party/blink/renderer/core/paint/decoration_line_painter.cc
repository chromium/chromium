// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"

namespace blink {

namespace {

float RoundDownThickness(float stroke_thickness) {
  return std::max(floorf(stroke_thickness), 1.0f);
}

gfx::RectF DecorationRect(gfx::PointF pt, float width, float stroke_thickness) {
  return gfx::RectF(pt, gfx::SizeF(width, stroke_thickness));
}

gfx::RectF SnapYAxis(const gfx::RectF& decoration_rect) {
  gfx::RectF snapped = decoration_rect;
  snapped.set_y(floorf(decoration_rect.y() + 0.5f));
  snapped.set_height(RoundDownThickness(decoration_rect.height()));
  return snapped;
}

std::pair<gfx::Point, gfx::Point> GetSnappedPointsForTextLine(
    const gfx::RectF& decoration_rect) {
  int mid_y = floorf(decoration_rect.y() +
                     std::max(decoration_rect.height() / 2.0f, 0.5f));
  return {gfx::Point(decoration_rect.x(), mid_y),
          gfx::Point(decoration_rect.right(), mid_y)};
}

void DrawLineAsStroke(GraphicsContext& context,
                      const gfx::RectF& line_rect,
                      const StyledStrokeData& styled_stroke,
                      const AutoDarkMode& auto_dark_mode,
                      const cc::PaintFlags* paint_flags) {
  auto [start, end] = GetSnappedPointsForTextLine(line_rect);
  context.DrawLine(start, end, styled_stroke, auto_dark_mode, true,
                   paint_flags);
}

void DrawLineAsRect(GraphicsContext& context,
                    const gfx::RectF& line_rect,
                    const AutoDarkMode& auto_dark_mode,
                    const cc::PaintFlags* paint_flags) {
  if (paint_flags) {
    // In SVG (inferred by a non-null `paint_flags`), we don't snap the line
    // to get better scaling behavior. See crbug.com/1270336.
    context.DrawRect(gfx::RectFToSkRect(line_rect), *paint_flags,
                     auto_dark_mode);
  } else {
    // Avoid anti-aliasing lines. Currently, these are always horizontal.
    // Round to nearest pixel to match text and other content.
    const gfx::RectF snapped_line_rect = SnapYAxis(line_rect);

    cc::PaintFlags flags = context.FillFlags();
    // Text lines are drawn using the stroke color.
    flags.setColor(context.StrokeFlags().getColor4f());
    context.DrawRect(gfx::RectFToSkRect(snapped_line_rect), flags,
                     auto_dark_mode);
  }
}

}  // namespace

void DecorationLinePainter::DrawLineForText(
    GraphicsContext& context,
    const gfx::PointF& pt,
    float width,
    const StyledStrokeData& styled_stroke,
    const AutoDarkMode& auto_dark_mode,
    const cc::PaintFlags* paint_flags) {
  if (width <= 0) {
    return;
  }
  DrawLineForText(context, DecorationRect(pt, width, styled_stroke.Thickness()),
                  styled_stroke, auto_dark_mode, paint_flags);
}

void DecorationLinePainter::DrawLineForText(
    GraphicsContext& context,
    const gfx::RectF& line_rect,
    const StyledStrokeData& styled_stroke,
    const AutoDarkMode& auto_dark_mode,
    const cc::PaintFlags* paint_flags) {
  CHECK_GT(line_rect.width(), 0);
  switch (styled_stroke.Style()) {
    case kSolidStroke:
    case kDoubleStroke:
      DrawLineAsRect(context, line_rect, auto_dark_mode, paint_flags);
      break;
    case kDottedStroke:
    case kDashedStroke:
      DrawLineAsStroke(context, line_rect, styled_stroke, auto_dark_mode,
                       paint_flags);
      break;
    case kWavyStroke:
      NOTREACHED();
  }
}

gfx::RectF DecorationLinePainter::Bounds(
    const TextDecorationInfo& decoration_info) {
  const gfx::PointF start_point = decoration_info.StartPoint();
  switch (decoration_info.StrokeStyle()) {
    case kDottedStroke:
    case kDashedStroke: {
      const gfx::RectF line_rect =
          DecorationRect(start_point, decoration_info.Width(),
                         decoration_info.ResolvedThickness());
      const float thickness = roundf(line_rect.height());
      auto [start, end] = GetSnappedPointsForTextLine(line_rect);
      return gfx::RectF(start.x(), start.y() - thickness / 2,
                        end.x() - start.x(), thickness);
    }
    case kWavyStroke:
      // Returns the wavy bounds, which is the same size as the wavy paint rect
      // but at the origin needed by the actual decoration, for the global
      // transform.
      return decoration_info.WavyPaintRect();
    case kDoubleStroke: {
      const float double_offset = decoration_info.DoubleOffset();
      const float thickness = decoration_info.ResolvedThickness();
      if (double_offset > 0) {
        return gfx::RectF(start_point.x(), start_point.y(),
                          decoration_info.Width(), double_offset + thickness);
      }
      return gfx::RectF(start_point.x(), start_point.y() + double_offset,
                        decoration_info.Width(), -double_offset + thickness);
    }
    case kSolidStroke:
      return DecorationRect(start_point, decoration_info.Width(),
                            decoration_info.ResolvedThickness());
  }
}

void DecorationLinePainter::Paint(const Color& color,
                                  const cc::PaintFlags* flags) {
  StyledStrokeData styled_stroke;
  styled_stroke.SetStyle(decoration_info_.StrokeStyle());
  styled_stroke.SetThickness(decoration_info_.ResolvedThickness());

  context_.SetStrokeColor(color);

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(decoration_info_.TargetStyle(),
                        DarkModeFilter::ElementRole::kForeground));

  // TODO(crbug.com/1346281) make other decoration styles work with PaintFlags
  switch (decoration_info_.DecorationStyle()) {
    case ETextDecorationStyle::kWavy:
      PaintWavyTextDecoration(auto_dark_mode);
      break;
    case ETextDecorationStyle::kDotted:
    case ETextDecorationStyle::kDashed:
      context_.SetShouldAntialias(decoration_info_.ShouldAntialias());
      [[fallthrough]];
    default:
      DrawLineForText(context_, decoration_info_.StartPoint(),
                      decoration_info_.Width(), styled_stroke, auto_dark_mode,
                      flags);

      if (decoration_info_.DecorationStyle() == ETextDecorationStyle::kDouble) {
        DrawLineForText(context_,
                        decoration_info_.StartPoint() +
                            gfx::Vector2dF(0, decoration_info_.DoubleOffset()),
                        decoration_info_.Width(), styled_stroke, auto_dark_mode,
                        flags);
      }
  }
}

void DecorationLinePainter::PaintWavyTextDecoration(
    const AutoDarkMode& auto_dark_mode) {
  // The wavy paint rect, which has the height of the wavy tile rect but the
  // width needed by the actual decoration, for the DrawRect operation.
  const gfx::RectF paint_rect = decoration_info_.WavyPaintRect();

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(PaintShader::MakePaintRecord(
      decoration_info_.WavyTileRecord(),
      gfx::RectFToSkRect(decoration_info_.WavyTileRect()), SkTileMode::kRepeat,
      SkTileMode::kDecal, nullptr));

  GraphicsContextStateSaver state_saver(context_);
  context_.Translate(paint_rect.x(), paint_rect.y());
  context_.DrawRect(gfx::RectFToSkRect(gfx::RectF(paint_rect.size())), flags,
                    auto_dark_mode);
}

}  // namespace blink
