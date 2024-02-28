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

gfx::RectF GetRectForTextLine(gfx::PointF pt,
                              float width,
                              float stroke_thickness) {
  // Avoid anti-aliasing lines. Currently, these are always horizontal.
  // Round to nearest pixel to match text and other content.
  float y = floorf(pt.y() + 0.5f);
  return gfx::RectF(pt.x(), y, width, stroke_thickness);
}

std::pair<gfx::Point, gfx::Point> GetPointsForTextLine(gfx::PointF pt,
                                                       float width,
                                                       float stroke_thickness) {
  int y = floorf(pt.y() + std::max<float>(stroke_thickness / 2.0f, 0.5f));
  return {gfx::Point(pt.x(), y), gfx::Point(pt.x() + width, y)};
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

  auto stroke_style = styled_stroke.Style();
  const float thickness = styled_stroke.Thickness();
  DCHECK_NE(stroke_style, kWavyStroke);
  if (ShouldUseStrokeForTextLine(stroke_style)) {
    auto [start, end] = GetPointsForTextLine(pt, width, thickness);
    context.DrawLine(start, end, styled_stroke, auto_dark_mode, true,
                     paint_flags);
  } else {
    if (paint_flags) {
      // In SVG, we don't round down the thickness to an integer for better
      // scaling behavior.  See crbug.com/1270336.
      SkRect r = gfx::RectFToSkRect(GetRectForTextLine(pt, width, thickness));
      context.DrawRect(r, *paint_flags, auto_dark_mode);
    } else {
      cc::PaintFlags flags = context.FillFlags();
      // Text lines are drawn using the stroke color.
      flags.setColor(context.StrokeFlags().getColor4f());
      SkRect r = gfx::RectFToSkRect(
          GetRectForTextLine(pt, width, RoundDownThickness(thickness)));
      context.DrawRect(r, flags, auto_dark_mode);
    }
  }
}

Path DecorationLinePainter::GetPathForTextLine(const gfx::PointF& pt,
                                               float width,
                                               float stroke_thickness,
                                               StrokeStyle stroke_style) {
  Path path;
  DCHECK_NE(stroke_style, kWavyStroke);
  if (ShouldUseStrokeForTextLine(stroke_style)) {
    auto [start, end] = GetPointsForTextLine(pt, width, stroke_thickness);
    path.MoveTo(gfx::PointF(start));
    path.AddLineTo(gfx::PointF(end));
  } else {
    path.AddRect(
        GetRectForTextLine(pt, width, RoundDownThickness(stroke_thickness)));
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
      PaintWavyTextDecoration();
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

void DecorationLinePainter::PaintWavyTextDecoration() {
  // We need this because of the clipping we're doing below, as we paint both
  // overlines and underlines here. That clip would hide the overlines, when
  // painting the underlines.
  GraphicsContextStateSaver state_saver(context_);

  context_.SetShouldAntialias(true);

  // The wavy line is larger than the line, as we add whole waves before and
  // after the line in TextDecorationInfo::PrepareWavyStrokePath().
  gfx::PointF origin = decoration_info_.Bounds().origin();

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(decoration_info_.TargetStyle(),
                        DarkModeFilter::ElementRole::kForeground));
  cc::PaintFlags flags;

  flags.setAntiAlias(true);
  flags.setShader(PaintShader::MakePaintRecord(
      decoration_info_.WavyTileRecord(),
      gfx::RectFToSkRect(decoration_info_.WavyTileRect()), SkTileMode::kRepeat,
      SkTileMode::kDecal, nullptr));
  context_.Translate(origin.x(), origin.y());
  context_.DrawRect(gfx::RectFToSkRect(decoration_info_.WavyPaintRect()), flags,
                    auto_dark_mode);
}

}  // namespace blink
