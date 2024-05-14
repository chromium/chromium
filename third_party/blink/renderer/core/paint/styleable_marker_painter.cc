// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/styleable_marker_painter.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"
#include "third_party/skia/include/core/SkPath.h"

namespace blink {

namespace {

#if !BUILDFLAG(IS_APPLE)

static const float kMarkerWidth = 4;
static const float kMarkerHeight = 2;

PaintRecord RecordMarker(Color blink_color) {
  const SkColor color = blink_color.Rgb();

  // Record the path equivalent to this legacy pattern:
  //   X o   o X o   o X
  //     o X o   o X o

  // Adjust the phase such that f' == 0 is "pixel"-centered
  // (for optimal rasterization at native rez).
  SkPath path;
  path.moveTo(kMarkerWidth * -3 / 8, kMarkerHeight * 3 / 4);
  path.cubicTo(kMarkerWidth * -1 / 8, kMarkerHeight * 3 / 4,
               kMarkerWidth * -1 / 8, kMarkerHeight * 1 / 4,
               kMarkerWidth * 1 / 8, kMarkerHeight * 1 / 4);
  path.cubicTo(kMarkerWidth * 3 / 8, kMarkerHeight * 1 / 4,
               kMarkerWidth * 3 / 8, kMarkerHeight * 3 / 4,
               kMarkerWidth * 5 / 8, kMarkerHeight * 3 / 4);
  path.cubicTo(kMarkerWidth * 7 / 8, kMarkerHeight * 3 / 4,
               kMarkerWidth * 7 / 8, kMarkerHeight * 1 / 4,
               kMarkerWidth * 9 / 8, kMarkerHeight * 1 / 4);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kMarkerHeight * 1 / 2);

  PaintRecorder recorder;
  recorder.beginRecording();
  recorder.getRecordingCanvas()->drawPath(path, flags);

  return recorder.finishRecordingAsPicture();
}

#else  // !BUILDFLAG(IS_APPLE)

static const float kMarkerWidth = 4;
static const float kMarkerHeight = 3;
// Spacing between two dots.
static const float kMarkerSpacing = 1;

PaintRecord RecordMarker(Color blink_color) {
  const SkColor color = blink_color.Rgb();

  // Match the artwork used by the Mac.
  static const float kR = 1.5f;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  PaintRecorder recorder;
  recorder.beginRecording();
  recorder.getRecordingCanvas()->drawOval(SkRect::MakeWH(2 * kR, 2 * kR),
                                          flags);
  return recorder.finishRecordingAsPicture();
}

#endif  // !BUILDFLAG(IS_APPLE)

void DrawDocumentMarker(GraphicsContext& context,
                        const gfx::PointF& pt,
                        float width,
                        float zoom,
                        PaintRecord marker) {
  // Position already includes zoom and device scale factor.
  SkScalar origin_x = WebCoreFloatToSkScalar(pt.x());
  SkScalar origin_y = WebCoreFloatToSkScalar(pt.y());

#if BUILDFLAG(IS_APPLE)
  // Make sure to draw only complete dots, and finish inside the marked text.
  float spacing = kMarkerSpacing * zoom;
  width -= fmodf(width + spacing, kMarkerWidth * zoom) - spacing;
#endif

  const auto rect = SkRect::MakeWH(width, kMarkerHeight * zoom);
  const auto local_matrix = SkMatrix::Scale(zoom, zoom);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(PaintShader::MakePaintRecord(
      std::move(marker), SkRect::MakeWH(kMarkerWidth, kMarkerHeight),
      SkTileMode::kRepeat, SkTileMode::kClamp, &local_matrix));

  // Apply the origin translation as a global transform.  This ensures that the
  // shader local matrix depends solely on zoom => Skia can reuse the same
  // cached tile for all markers at a given zoom level.
  GraphicsContextStateSaver saver(context);
  context.Translate(origin_x, origin_y);
  context.DrawRect(rect, flags, AutoDarkMode::Disabled());
}

}  // namespace

bool StyleableMarkerPainter::ShouldPaintUnderline(
    const StyleableMarker& marker) {
  if (marker.HasThicknessNone() ||
      (marker.UnderlineColor() == Color::kTransparent &&
       !marker.UseTextColor()) ||
      marker.UnderlineStyle() == ui::mojom::blink::ImeTextSpanUnderlineStyle::kNone) {
    return false;
  }
  return true;
}

void StyleableMarkerPainter::PaintUnderline(const StyleableMarker& marker,
                                            GraphicsContext& context,
                                            const PhysicalOffset& box_origin,
                                            const ComputedStyle& style,
                                            const LineRelativeRect& marker_rect,
                                            LayoutUnit logical_height,
                                            bool in_dark_mode) {
  // start of line to draw, relative to box_origin.X()
  LayoutUnit start = LayoutUnit(marker_rect.LineLeft());
  LayoutUnit width = LayoutUnit(marker_rect.InlineSize());

  // We need to have some space between underlines of subsequent clauses,
  // because some input methods do not use different underline styles for those.
  // We make each line shorter, which has a harmless side effect of shortening
  // the first and last clauses, too.
  start += 1;
  width -= 2;

  // Thick marked text underlines are 2px (before zoom) thick as long as there
  // is room for the 2px line under the baseline.  All other marked text
  // underlines are 1px (before zoom) thick.  If there's not enough space the
  // underline will touch or overlap characters. Line thickness should change
  // with zoom.
  int line_thickness = 1 * style.EffectiveZoom();
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  int baseline = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  if (marker.HasThicknessThick()) {
    int thick_line_thickness = 2 * style.EffectiveZoom();
    if (logical_height.ToInt() - baseline >= thick_line_thickness)
      line_thickness = thick_line_thickness;
  }

  Color marker_color =
      (marker.UseTextColor() || in_dark_mode)
          ? style.VisitedDependentColor(GetCSSPropertyWebkitTextFillColor())
          : marker.UnderlineColor();

  using UnderlineStyle = ui::mojom::blink::ImeTextSpanUnderlineStyle;
  if (marker.UnderlineStyle() != UnderlineStyle::kSquiggle) {
    StyledStrokeData styled_stroke;
    styled_stroke.SetThickness(line_thickness);
    // Set the style of the underline if there is any.
    switch (marker.UnderlineStyle()) {
      case UnderlineStyle::kDash:
        styled_stroke.SetStyle(StrokeStyle::kDashedStroke);
        break;
      case UnderlineStyle::kDot:
        styled_stroke.SetStyle(StrokeStyle::kDottedStroke);
        break;
      case UnderlineStyle::kSolid:
        styled_stroke.SetStyle(StrokeStyle::kSolidStroke);
        break;
      case UnderlineStyle::kSquiggle:
        // Wavy stroke style is not implemented in DrawLineForText so we handle
        // it specially in the else condition below only for composition
        // markers.
      case UnderlineStyle::kNone:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    context.SetStrokeColor(marker_color);

    DecorationLinePainter::DrawLineForText(
        context,
        gfx::PointF(box_origin.left + start,
                    (box_origin.top + logical_height.ToInt() - line_thickness)
                        .ToFloat()),
        width, styled_stroke,
        PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));
  } else {
    // For wavy underline format we use this logic that is very similar to
    // spelling/grammar squiggles format. Only applicable for composition
    // markers for now.
    if (marker.GetType() == DocumentMarker::kComposition) {
      PaintRecord composition_marker = RecordMarker(marker_color);
      DrawDocumentMarker(
          context,
          gfx::PointF((box_origin.left + start).ToFloat(),
                      (box_origin.top + logical_height.ToInt() - line_thickness)
                          .ToFloat()),
          width, line_thickness, std::move(composition_marker));
    }
  }
}

}  // namespace blink
