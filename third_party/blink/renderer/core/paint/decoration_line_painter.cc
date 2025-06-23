// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"

namespace blink {

namespace {

float RoundDownThickness(float stroke_thickness) {
  return std::max(floorf(stroke_thickness), 1.0f);
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

struct WavyParams {
  float resolved_thickness;
  float effective_zoom;
  bool spelling_grammar;
  Color color;

  bool operator==(const WavyParams&) const = default;
  DISALLOW_NEW();
};

float WavyControlPointDistance(const WavyParams& params) {
  // Distance between decoration's axis and Bezier curve's control points. The
  // height of the curve is based on this distance. Increases the curve's height
  // as strokeThickness increases to make the curve look better.
  if (params.spelling_grammar) {
    return 5 * params.effective_zoom;
  }
  // Setting the distance to half-pixel values gives better antialiasing
  // results, particularly for small values.
  return 0.5 + roundf(3 * std::max<float>(1, params.resolved_thickness) + 0.5);
}

float WavyStep(const WavyParams& params) {
  // Increment used to form the diamond shape between start point (p1), control
  // points and end point (p2) along the axis of the decoration. Makes the curve
  // wider as strokeThickness increases to make the curve look better.
  if (params.spelling_grammar) {
    return 3 * params.effective_zoom;
  }
  // Setting the step to half-pixel values gives better antialiasing
  // results, particularly for small values.
  return 0.5 + roundf(2 * std::max<float>(1, params.resolved_thickness) + 0.5);
}

// Computes the wavy pattern rect, which is where the desired wavy pattern would
// be found when painting the wavy stroke path at the origin, or in other words,
// how far PrepareWavyTileRecord needs to translate in the opposite direction
// when painting to ensure that nothing is painted at y<0.
gfx::RectF ComputeWavyPatternRect(const WavyParams& params,
                                  const Path& stroke_path) {
  StrokeData stroke_data;
  stroke_data.SetThickness(params.resolved_thickness);

  // Expand the stroke rect to integer y coordinates in both directions, to
  // avoid messing with the vertical antialiasing.
  gfx::RectF stroke_rect = stroke_path.StrokeBoundingRect(stroke_data);
  float top = floorf(stroke_rect.y());
  float bottom = ceilf(stroke_rect.bottom());
  return {0.f, top, 2.f * WavyStep(params), bottom - top};
}

// Prepares a path for a cubic Bezier curve repeated three times, yielding a
// wavy pattern that we can cut into a tiling shader (PrepareWavyTileRecord).
//
// The result ignores the local origin, line offset, and (wavy) double offset,
// so the midpoints are always at y=0.5, while the phase is shifted for either
// wavy or spelling/grammar decorations so the desired pattern starts at x=0.
//
// The start point, control points (cp1 and cp2), and end point of each curve
// form a diamond shape:
//
//            cp2                      cp2                      cp2
// ---         +                        +                        +
// |               x=0
// | control         |--- spelling/grammar ---|
// | point          . .                      . .                      . .
// | distance     .     .                  .     .                  .     .
// |            .         .              .         .              .         .
// +-- y=0.5   .            +           .            +           .            +
//  .         .              .         .              .         .
//    .     .                  .     .                  .     .
//      . .                      . .                      . .
//                          |-------- other ---------|
//                        x=0
//             +                        +                        +
//            cp1                      cp1                      cp1
// |-----------|------------|
//     step         step
Path PrepareWavyStrokePath(const WavyParams& params) {
  float control_point_distance = WavyControlPointDistance(params);
  float step = WavyStep(params);

  // We paint the wave before and after the text line (to cover the whole length
  // of the line) and then we clip it at
  // AppliedDecorationPainter::StrokeWavyTextDecoration().
  // Offset the start point, so the bezier curve starts before the current line,
  // that way we can clip it exactly the same way in both ends.
  // For spelling and grammar errors we offset by half a step less, to get a
  // result closer to Microsoft Word circa 2021.
  float phase_shift = (params.spelling_grammar ? -1.5f : -2.f) * step;

  // Midpoints at y=0.5, to reduce vertical antialiasing.
  gfx::PointF start{phase_shift, 0.5f};
  gfx::PointF end{start + gfx::Vector2dF(2.f * step, 0.0f)};
  gfx::PointF cp1{start + gfx::Vector2dF(step, +control_point_distance)};
  gfx::PointF cp2{start + gfx::Vector2dF(step, -control_point_distance)};

  PathBuilder result;
  result.MoveTo(start);

  result.CubicTo(cp1, cp2, end);
  cp1.set_x(cp1.x() + 2.f * step);
  cp2.set_x(cp2.x() + 2.f * step);
  end.set_x(end.x() + 2.f * step);
  result.CubicTo(cp1, cp2, end);
  cp1.set_x(cp1.x() + 2.f * step);
  cp2.set_x(cp2.x() + 2.f * step);
  end.set_x(end.x() + 2.f * step);
  result.CubicTo(cp1, cp2, end);

  return result.Finalize();
}

cc::PaintRecord PrepareWavyTileRecord(const WavyParams& params,
                                      const Path& stroke_path,
                                      const gfx::RectF& pattern_rect) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(params.color.Rgb());
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(params.resolved_thickness);

  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();

  // Translate the wavy pattern so that nothing is painted at y<0.
  canvas->translate(-pattern_rect.x(), -pattern_rect.y());
  canvas->drawPath(stroke_path.GetSkPath(), flags);

  return recorder.finishRecordingAsPicture();
}

void ComputeWavyLineData(const WavyParams& params,
                         DecorationGeometry& geometry) {
  struct WavyCache {
    WavyParams key;
    gfx::RectF pattern_rect;
    cc::PaintRecord tile_record;
    DISALLOW_NEW();
  };

  DEFINE_STATIC_LOCAL(std::optional<WavyCache>, wavy_cache, (std::nullopt));

  if (wavy_cache && wavy_cache->key == params) {
    geometry.wavy_pattern_rect = wavy_cache->pattern_rect;
    geometry.wavy_tile_record = wavy_cache->tile_record;
    return;
  }

  Path stroke_path = PrepareWavyStrokePath(params);
  geometry.wavy_pattern_rect = ComputeWavyPatternRect(params, stroke_path);
  geometry.wavy_tile_record =
      PrepareWavyTileRecord(params, stroke_path, geometry.wavy_pattern_rect);
  wavy_cache =
      WavyCache{params, geometry.wavy_pattern_rect, geometry.wavy_tile_record};
}

// Returns the wavy paint rect, which has the height of the wavy tile rect but
// the width needed by the actual decoration, for the DrawRect operation.
gfx::RectF WavyPaintRect(const DecorationGeometry& geometry) {
  // The offset from the local origin is the (wavy) double offset and the
  // origin of the wavy pattern rect (around minus half the amplitude).
  gfx::PointF origin =
      geometry.line.origin() + geometry.wavy_pattern_rect.OffsetFromOrigin() +
      gfx::Vector2dF{0.f, geometry.double_offset * geometry.wavy_offset_factor};
  // Get the height of the wavy tile, and the width of the decoration.
  gfx::SizeF size(geometry.line.width(), geometry.wavy_pattern_rect.height());
  return {origin, size};
}

}  // namespace

DecorationGeometry DecorationGeometry::Make(StrokeStyle style,
                                            const gfx::RectF& line,
                                            float zoom,
                                            float double_offset,
                                            int wavy_offset_factor,
                                            bool is_spelling_or_grammar,
                                            const Color& line_color) {
  DecorationGeometry geometry;
  geometry.style = style;
  geometry.line = line;
  geometry.double_offset = double_offset;

  if (geometry.style == kWavyStroke) {
    WavyParams params{geometry.Thickness(), zoom, is_spelling_or_grammar,
                      line_color};
    ComputeWavyLineData(params, geometry);
    geometry.wavy_offset_factor = wavy_offset_factor;
  }
  return geometry;
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

gfx::RectF DecorationLinePainter::Bounds(const DecorationGeometry& geometry) {
  switch (geometry.style) {
    case kDottedStroke:
    case kDashedStroke: {
      const float thickness = roundf(geometry.Thickness());
      auto [start, end] = GetSnappedPointsForTextLine(geometry.line);
      return gfx::RectF(start.x(), start.y() - thickness / 2,
                        end.x() - start.x(), thickness);
    }
    case kWavyStroke:
      // Returns the wavy bounds, which is the same size as the wavy paint rect
      // but at the origin needed by the actual decoration, for the global
      // transform.
      return WavyPaintRect(geometry);
    case kDoubleStroke: {
      gfx::RectF double_line_rect = geometry.line;
      if (geometry.double_offset < 0) {
        double_line_rect.set_y(double_line_rect.y() + geometry.double_offset);
      }
      double_line_rect.set_height(double_line_rect.height() +
                                  std::abs(geometry.double_offset));
      return double_line_rect;
    }
    case kSolidStroke:
      return geometry.line;
  }
}

void DecorationLinePainter::Paint(const Color& color,
                                  const cc::PaintFlags* flags) {
  const DecorationGeometry& geometry = decoration_info_.GetGeometry();
  if (geometry.line.width() <= 0) {
    return;
  }

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(decoration_info_.TargetStyle(),
                        DarkModeFilter::ElementRole::kForeground));

  // TODO(crbug.com/1346281) make other decoration styles work with PaintFlags
  switch (geometry.style) {
    case kWavyStroke:
      PaintWavyTextDecoration(geometry, auto_dark_mode);
      break;
    case kDottedStroke:
    case kDashedStroke:
      context_.SetShouldAntialias(geometry.antialias);
      [[fallthrough]];
    case kSolidStroke:
    case kDoubleStroke: {
      StyledStrokeData styled_stroke;
      styled_stroke.SetStyle(geometry.style);
      styled_stroke.SetThickness(geometry.Thickness());

      context_.SetStrokeColor(color);

      DrawLineForText(context_, geometry.line, styled_stroke, auto_dark_mode,
                      flags);

      if (geometry.style == kDoubleStroke) {
        const gfx::RectF second_line_rect =
            geometry.line + gfx::Vector2dF(0, geometry.double_offset);
        DrawLineAsRect(context_, second_line_rect, auto_dark_mode, flags);
      }
      break;
    }
  }
}

void DecorationLinePainter::PaintWavyTextDecoration(
    const DecorationGeometry& geometry,
    const AutoDarkMode& auto_dark_mode) {
  // The wavy paint rect, which has the height of the wavy tile rect but the
  // width needed by the actual decoration, for the DrawRect operation.
  const gfx::RectF paint_rect = WavyPaintRect(geometry);
  // The wavy tile rect is the same size as the wavy pattern rect but at origin
  // (0,0).
  const gfx::RectF tile_rect(geometry.wavy_pattern_rect.size());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(PaintShader::MakePaintRecord(
      geometry.wavy_tile_record, gfx::RectFToSkRect(tile_rect),
      SkTileMode::kRepeat, SkTileMode::kDecal, nullptr));

  GraphicsContextStateSaver state_saver(context_);
  context_.Translate(paint_rect.x(), paint_rect.y());
  context_.DrawRect(gfx::RectFToSkRect(gfx::RectF(paint_rect.size())), flags,
                    auto_dark_mode);
}

}  // namespace blink
