// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
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

bool ShouldUseStrokeForTextLine(StrokeStyle stroke_style) {
  switch (stroke_style) {
    case kNoStroke:
    case kSolidStroke:
    case kDoubleStroke:
      return false;
    case kDottedStroke:
    case kDashedStroke:
    case kWavyStroke:
    default:
      return true;
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

  gfx::RectF line_rect = DecorationRect(pt, width, styled_stroke.Thickness());

  auto stroke_style = styled_stroke.Style();
  DCHECK_NE(stroke_style, kWavyStroke);
  if (ShouldUseStrokeForTextLine(stroke_style)) {
    auto [start, end] = GetSnappedPointsForTextLine(line_rect);
    context.DrawLine(start, end, styled_stroke, auto_dark_mode, true,
                     paint_flags);
  } else {
    if (paint_flags) {
      // In SVG (inferred by a non-null `paint_flags`), we don't snap the line
      // to get better scaling behavior. See crbug.com/1270336.
      context.DrawRect(gfx::RectFToSkRect(line_rect), *paint_flags,
                       auto_dark_mode);
    } else {
      // Avoid anti-aliasing lines. Currently, these are always horizontal.
      // Round to nearest pixel to match text and other content.
      line_rect = SnapYAxis(line_rect);

      cc::PaintFlags flags = context.FillFlags();
      // Text lines are drawn using the stroke color.
      flags.setColor(context.StrokeFlags().getColor4f());
      context.DrawRect(gfx::RectFToSkRect(line_rect), flags, auto_dark_mode);
    }
  }
}

Path DecorationLinePainter::GetPathForTextLine(const gfx::PointF& pt,
                                               float width,
                                               float stroke_thickness,
                                               StrokeStyle stroke_style) {
  DCHECK_NE(stroke_style, kWavyStroke);
  const gfx::RectF line_rect = DecorationRect(pt, width, stroke_thickness);
  Path path;
  if (ShouldUseStrokeForTextLine(stroke_style)) {
    auto [start, end] = GetSnappedPointsForTextLine(line_rect);
    path.MoveTo(gfx::PointF(start));
    path.AddLineTo(gfx::PointF(end));
  } else {
    path.AddRect(SnapYAxis(line_rect));
  }
  return path;
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
  // The wavy line is larger than the line, as we add whole waves before and
  // after the line in TextDecorationInfo::PrepareWavyStrokePath().
  gfx::PointF origin = decoration_info_.Bounds().origin();

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(PaintShader::MakePaintRecord(
      decoration_info_.WavyTileRecord(),
      gfx::RectFToSkRect(decoration_info_.WavyTileRect()), SkTileMode::kRepeat,
      SkTileMode::kDecal, nullptr));

  // We need this because of the clipping we're doing below, as we paint both
  // overlines and underlines here. That clip would hide the overlines, when
  // painting the underlines.
  GraphicsContextStateSaver state_saver(context_);
  context_.SetShouldAntialias(true);
  context_.Translate(origin.x(), origin.y());
  context_.DrawRect(gfx::RectFToSkRect(decoration_info_.WavyPaintRect()), flags,
                    auto_dark_mode);
}

}  // namespace blink
