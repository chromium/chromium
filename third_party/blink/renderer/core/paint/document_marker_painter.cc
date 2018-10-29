// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/document_marker_painter.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"

namespace blink {

namespace {

#if !defined(OS_MACOSX)

static const float kMarkerWidth = 4;
static const float kMarkerHeight = 2;

sk_sp<PaintRecord> RecordMarker(Color blink_color) {
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

  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kMarkerHeight * 1 / 2);

  PaintRecorder recorder;
  recorder.beginRecording(kMarkerWidth, kMarkerHeight);
  recorder.getRecordingCanvas()->drawPath(path, flags);

  return recorder.finishRecordingAsPicture();
}

#else  // defined(OS_MACOSX)

static const float kMarkerWidth = 4;
static const float kMarkerHeight = 3;

sk_sp<PaintRecord> RecordMarker(Color blink_color) {
  const SkColor color = blink_color.Rgb();

  // Match the artwork used by the Mac.
  static const float kR = 1.5f;

  // top->bottom translucent gradient.
  const SkColor colors[2] = {
      SkColorSetARGB(0x48, SkColorGetR(color), SkColorGetG(color),
                     SkColorGetB(color)),
      color};
  const SkPoint pts[2] = {SkPoint::Make(0, 0), SkPoint::Make(0, 2 * kR)};

  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setShader(PaintShader::MakeLinearGradient(
      pts, colors, nullptr, ARRAY_SIZE(colors), SkShader::kClamp_TileMode));
  PaintRecorder recorder;
  recorder.beginRecording(kMarkerWidth, kMarkerHeight);
  recorder.getRecordingCanvas()->drawOval(SkRect::MakeWH(2 * kR, 2 * kR),
                                          flags);
  return recorder.finishRecordingAsPicture();
}

#endif  // defined(OS_MACOSX)

void DrawDocumentMarker(GraphicsContext& context,
                        const FloatPoint& pt,
                        float width,
                        DocumentMarker::MarkerType marker_type,
                        float zoom) {
  DCHECK(marker_type == DocumentMarker::kSpelling ||
         marker_type == DocumentMarker::kGrammar);

  DEFINE_STATIC_LOCAL(
      PaintRecord*, spelling_marker,
      (RecordMarker(
           LayoutTheme::GetTheme().PlatformSpellingMarkerUnderlineColor())
           .release()));
  DEFINE_STATIC_LOCAL(
      PaintRecord*, grammar_marker,
      (RecordMarker(
           LayoutTheme::GetTheme().PlatformGrammarMarkerUnderlineColor())
           .release()));
  auto* const marker = marker_type == DocumentMarker::kSpelling
                           ? spelling_marker
                           : grammar_marker;

  // Position already includes zoom and device scale factor.
  SkScalar origin_x = WebCoreFloatToSkScalar(pt.X());
  SkScalar origin_y = WebCoreFloatToSkScalar(pt.Y());

#if defined(OS_MACOSX)
  // Make sure to draw only complete dots, and finish inside the marked text.
  width -= fmodf(width, kMarkerWidth * zoom);
#else
  // Offset it vertically by 1 so that there's some space under the text.
  origin_y += 1;
#endif

  const auto rect = SkRect::MakeWH(width, kMarkerHeight * zoom);
  const auto local_matrix = SkMatrix::MakeScale(zoom, zoom);

  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(PaintShader::MakePaintRecord(
      sk_ref_sp(marker), FloatRect(0, 0, kMarkerWidth, kMarkerHeight),
      SkShader::kRepeat_TileMode, SkShader::kClamp_TileMode, &local_matrix));

  // Apply the origin translation as a global transform.  This ensures that the
  // shader local matrix depends solely on zoom => Skia can reuse the same
  // cached tile for all markers at a given zoom level.
  GraphicsContextStateSaver saver(context);
  context.Translate(origin_x, origin_y);
  context.DrawRect(rect, flags);
}

}  // anonymous ns

void DocumentMarkerPainter::PaintStyleableMarkerUnderline(
    GraphicsContext& context,
    const LayoutPoint& box_origin,
    const StyleableMarker& marker,
    const ComputedStyle& style,
    const FloatRect& marker_rect,
    LayoutUnit logical_height) {
  if (marker.HasThicknessNone() ||
      (marker.UnderlineColor() == Color::kTransparent &&
       !marker.UseTextColor()))
    return;

  // start of line to draw, relative to box_origin.X()
  LayoutUnit start = LayoutUnit(marker_rect.X());
  LayoutUnit width = LayoutUnit(marker_rect.Width());

  // We need to have some space between underlines of subsequent clauses,
  // because some input methods do not use different underline styles for those.
  // We make each line shorter, which has a harmless side effect of shortening
  // the first and last clauses, too.
  start += 1;
  width -= 2;

  // Thick marked text underlines are 2px thick as long as there is room for the
  // 2px line under the baseline.  All other marked text underlines are 1px
  // thick.  If there's not enough space the underline will touch or overlap
  // characters.
  int line_thickness = 1;
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  int baseline = font_data ? font_data->GetFontMetrics().Ascent() : 0;
  if (marker.HasThicknessThick() && logical_height.ToInt() - baseline >= 2)
    line_thickness = 2;

  // Line thickness should change with zoom.
  line_thickness *= style.EffectiveZoom();

  Color marker_color =
      marker.UseTextColor()
          ? style.VisitedDependentColor(GetCSSPropertyWebkitTextFillColor())
          : marker.UnderlineColor();
  context.SetStrokeColor(marker_color);

  context.SetStrokeThickness(line_thickness);
  context.DrawLineForText(
      FloatPoint(
          box_origin.X() + start,
          (box_origin.Y() + logical_height.ToInt() - line_thickness).ToFloat()),
      width);
}

static const int kMisspellingLineThickness = 3;

void DocumentMarkerPainter::PaintDocumentMarker(
    GraphicsContext& context,
    const LayoutPoint& box_origin,
    const ComputedStyle& style,
    DocumentMarker::MarkerType marker_type,
    const LayoutRect& local_rect) {
  // IMPORTANT: The misspelling underline is not considered when calculating the
  // text bounds, so we have to make sure to fit within those bounds.  This
  // means the top pixel(s) of the underline will overlap the bottom pixel(s) of
  // the glyphs in smaller font sizes.  The alternatives are to increase the
  // line spacing (bad!!) or decrease the underline thickness.  The overlap is
  // actually the most useful, and matches what AppKit does.  So, we generally
  // place the underline at the bottom of the text, but in larger fonts that's
  // not so good so we pin to two pixels under the baseline.
  int line_thickness = kMisspellingLineThickness;

  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  int baseline = font_data->GetFontMetrics().Ascent();
  int descent = (local_rect.Height() - baseline).ToInt();
  int underline_offset;
  if (descent <= (line_thickness + 2)) {
    // Place the underline at the very bottom of the text in small/medium fonts.
    underline_offset = (local_rect.Height() - line_thickness).ToInt();
  } else {
    // In larger fonts, though, place the underline up near the baseline to
    // prevent a big gap.
    underline_offset = baseline + 2;
  }
  DrawDocumentMarker(context,
                     FloatPoint((box_origin.X() + local_rect.X()).ToFloat(),
                                (box_origin.Y() + underline_offset).ToFloat()),
                     local_rect.Width().ToFloat(), marker_type,
                     style.EffectiveZoom());
}

TextPaintStyle DocumentMarkerPainter::ComputeTextPaintStyleFrom(
    const ComputedStyle& style,
    const TextMatchMarker& marker) {
  const Color text_color =
      LayoutTheme::GetTheme().PlatformTextSearchColor(marker.IsActiveMatch());
  if (style.VisitedDependentColor(GetCSSPropertyColor()) == text_color)
    return {};

  TextPaintStyle text_style;
  text_style.current_color = text_style.fill_color = text_style.stroke_color =
      text_style.emphasis_mark_color = text_color;
  text_style.stroke_width = style.TextStrokeWidth();
  text_style.shadow = nullptr;
  return text_style;
}
}  // namespace blink
