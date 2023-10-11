// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/document_marker_painter.h"

#include <algorithm>

#include "build/build_config.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/skia/include/core/SkPathBuilder.h"

namespace blink {

namespace {

#if !BUILDFLAG(IS_MAC)

static const float kMarkerWidth = 4;
static const float kMarkerHeight = 2;

PaintRecord RecordMarker(Color blink_color) {
  const SkColor color = blink_color.Rgb();

  // Record the path equivalent to this legacy pattern:
  //   X o   o X o   o X
  //     o X o   o X o

  // Adjust the phase such that f' == 0 is "pixel"-centered
  // (for optimal rasterization at native rez).
  SkPathBuilder path;
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
  recorder.getRecordingCanvas()->drawPath(path.detach(), flags);

  return recorder.finishRecordingAsPicture();
}

#else  // !BUILDFLAG(IS_MAC)

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

#endif  // !BUILDFLAG(IS_MAC)

void DrawDocumentMarker(GraphicsContext& context,
                        const gfx::PointF& pt,
                        float width,
                        float zoom,
                        PaintRecord marker) {
  // Position already includes zoom and device scale factor.
  SkScalar origin_x = WebCoreFloatToSkScalar(pt.x());
  SkScalar origin_y = WebCoreFloatToSkScalar(pt.y());

#if BUILDFLAG(IS_MAC)
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

bool DocumentMarkerPainter::ShouldPaintMarkerUnderline(
    const StyleableMarker& marker) {
  if (marker.HasThicknessNone() ||
      (marker.UnderlineColor() == Color::kTransparent &&
       !marker.UseTextColor()) ||
      marker.UnderlineStyle() == ui::mojom::ImeTextSpanUnderlineStyle::kNone) {
    return false;
  }
  return true;
}

void DocumentMarkerPainter::PaintStyleableMarkerUnderline(
    GraphicsContext& context,
    const PhysicalOffset& box_origin,
    const StyleableMarker& marker,
    const ComputedStyle& style,
    const Document& document,
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
  if (marker.UnderlineStyle() !=
      ui::mojom::ImeTextSpanUnderlineStyle::kSquiggle) {
    context.SetStrokeColor(marker_color);
    context.SetStrokeThickness(line_thickness);
    // Set the style of the underline if there is any.
    switch (marker.UnderlineStyle()) {
      case ui::mojom::ImeTextSpanUnderlineStyle::kDash:
        context.SetStrokeStyle(StrokeStyle::kDashedStroke);
        break;
      case ui::mojom::ImeTextSpanUnderlineStyle::kDot:
        context.SetStrokeStyle(StrokeStyle::kDottedStroke);
        break;
      case ui::mojom::ImeTextSpanUnderlineStyle::kSolid:
        context.SetStrokeStyle(StrokeStyle::kSolidStroke);
        break;
      case ui::mojom::ImeTextSpanUnderlineStyle::kNone:
        context.SetStrokeStyle(StrokeStyle::kNoStroke);
        break;
      case ui::mojom::ImeTextSpanUnderlineStyle::kSquiggle:
        // Wavy stroke style is not implemented in DrawLineForText so we handle
        // it specially in the else condition below only for composition
        // markers.
        break;
    }
    context.DrawLineForText(
        gfx::PointF(box_origin.left + start,
                    (box_origin.top + logical_height.ToInt() - line_thickness)
                        .ToFloat()),
        width,
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

void DocumentMarkerPainter::PaintDocumentMarker(
    const PaintInfo& paint_info,
    const PhysicalOffset& box_origin,
    const ComputedStyle& style,
    DocumentMarker::MarkerType marker_type,
    const LineRelativeRect& local_rect,
    absl::optional<Color> custom_marker_color) {
  // IMPORTANT: The misspelling underline is not considered when calculating the
  // text bounds, so we have to make sure to fit within those bounds.  This
  // means the top pixel(s) of the underline will overlap the bottom pixel(s) of
  // the glyphs in smaller font sizes.  The alternatives are to increase the
  // line spacing (bad!!) or decrease the underline thickness.  The overlap is
  // actually the most useful, and matches what AppKit does.  So, we generally
  // place the underline at the bottom of the text, but in larger fonts that's
  // not so good so we pin to two pixels under the baseline.
  float zoom = style.EffectiveZoom();
  int line_thickness = static_cast<int>(ceilf(kMarkerHeight * zoom));

  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  int baseline = font_data->GetFontMetrics().Ascent();
  int available_height = (local_rect.BlockSize() - baseline).ToInt();
  int underline_offset;
  if (available_height <= line_thickness + 2 * zoom) {
    // Place the underline at the very bottom of the text in small/medium fonts.
    // The underline will overlap with the bottom of the text if
    // available_height is smaller than line_thickness.
    underline_offset = (local_rect.BlockSize() - line_thickness).ToInt();
  } else {
    // In larger fonts, though, place the underline up near the baseline to
    // prevent a big gap.
    underline_offset = baseline + 2 * zoom;
  }

  DEFINE_STATIC_LOCAL(
      PaintRecord, spelling_marker,
      (RecordMarker(
          LayoutTheme::GetTheme().PlatformSpellingMarkerUnderlineColor())));
  DEFINE_STATIC_LOCAL(
      PaintRecord, grammar_marker,
      (RecordMarker(
          LayoutTheme::GetTheme().PlatformGrammarMarkerUnderlineColor())));

  PaintRecord marker = custom_marker_color ? RecordMarker(*custom_marker_color)
                       : marker_type == DocumentMarker::kSpelling
                           ? spelling_marker
                           : grammar_marker;

  DrawDocumentMarker(
      paint_info.context,
      gfx::PointF((box_origin.left + local_rect.LineLeft()).ToFloat(),
                  (box_origin.top + underline_offset).ToFloat()),
      local_rect.InlineSize().ToFloat(), zoom, marker);
}

TextPaintStyle DocumentMarkerPainter::ComputeTextPaintStyleFrom(
    const Document& document,
    Node* node,
    const ComputedStyle& style,
    const DocumentMarker& marker,
    const PaintInfo& paint_info) {
  Color text_color = style.VisitedDependentColor(GetCSSPropertyColor());
  if (marker.GetType() == DocumentMarker::kTextMatch) {
    const Color platform_text_color =
        LayoutTheme::GetTheme().PlatformTextSearchColor(
            To<TextMatchMarker>(marker).IsActiveMatch(),
            style.UsedColorScheme());
    if (platform_text_color == text_color)
      return {};
    text_color = platform_text_color;
  }

  TextPaintStyle text_style;
  text_style.current_color = text_style.fill_color = text_style.stroke_color =
      text_style.emphasis_mark_color = text_color;
  text_style.stroke_width = style.TextStrokeWidth();
  text_style.color_scheme = style.UsedColorScheme();
  text_style.shadow = nullptr;
  if (marker.GetType() == DocumentMarker::kTextMatch)
    return text_style;
  return HighlightStyleUtils::HighlightPaintingStyle(
      document, style, node, kPseudoIdTargetText, text_style, paint_info);
}
}  // namespace blink
