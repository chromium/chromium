// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_record.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

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

  const int thickness = roundf(styled_stroke.Thickness());
  StyledStrokeData::GeometryInfo geometry_info;
  geometry_info.path_length = end.x() - start.x();
  geometry_info.dash_thickness = thickness;
  const StrokeData stroke_data =
      styled_stroke.ConvertToStrokeData(geometry_info);

  gfx::PointF p1 = gfx::PointF(start);
  gfx::PointF p2 = gfx::PointF(end);
  // For odd widths, shift the line down by 0.5 to align it with the pixel grid
  // vertically.
  if (thickness % 2) {
    p1.set_y(p1.y() + 0.5f);
    p2.set_y(p2.y() + 0.5f);
  }

  if (!StyledStrokeData::StrokeIsDashed(thickness, styled_stroke.Style())) {
    // We draw thick dotted lines with 0 length dash strokes and round endcaps,
    // producing circles. The endcaps extend beyond the line's endpoints, so
    // move the start and end in.
    p1.set_x(p1.x() + thickness / 2.f);
    p2.set_x(p2.x() - thickness / 2.f);
  }

  if (paint_flags && paint_flags->getStyle() == cc::PaintFlags::kStroke_Style) {
    // Generate geometry based on `styled_stroke` and then paint that as a path
    // with provided paint flags.
    PathBuilder line_geometry;
    line_geometry.MoveTo(p1);
    line_geometry.LineTo(p2);
    const AffineTransform identity;
    const Path stroked_line =
        line_geometry.Finalize().StrokePath(stroke_data, identity);
    context.DrawPath(stroked_line.GetSkPath(), *paint_flags, auto_dark_mode);
  } else {
    // Transfer the line style to the paint.
    cc::PaintFlags flags = paint_flags ? *paint_flags : context.StrokeFlags();
    stroke_data.SetupPaint(&flags);
    context.DrawLine(p1, p2, flags, auto_dark_mode);
  }
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

// Builds a path of cubic Bezier curves (one per wavelength) along the
// horizontal axis, spanning `total_width` plus one wavelength of overhang
// on each side. Midpoints sit at y=0.5.
Path WavyCenterlinePath(const WaveDefinition& wave, float total_width) {
  PathBuilder result;
  float x = wave.phase;
  result.MoveTo({x, 0.5f});
  const float end_x = total_width + wave.wavelength;
  while (x < end_x) {
    result.CubicTo(
        {x + wave.wavelength * 0.5f, 0.5f + wave.control_point_distance},
        {x + wave.wavelength * 0.5f, 0.5f - wave.control_point_distance},
        {x + wave.wavelength, 0.5f});
    x += wave.wavelength;
  }
  return result.Finalize();
}

// Prepares a path for a cubic Bezier curve repeated three times, yielding a
// wavy pattern that we can cut into a tiling shader.
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
// |----- wavelength -------|
Path WavyPath(const WaveDefinition& wave) {
  // 3 cycles: the middle one is the visible tile; the flanking ones give the
  // tile shader continuous tangents at the seams.
  return WavyCenterlinePath(wave, /*total_width=*/wave.wavelength);
}

WaveDefinition MakeWave(float thickness) {
  const float clamped_thickness = std::max<float>(1, thickness);
  // Setting the step to half-pixel values gives better antialiasing results,
  // particularly for small values.
  const float wavelength = 1 + 2 * std::round(2 * clamped_thickness + 0.5f);
  // Setting the distance to half-pixel values gives better antialiasing
  // results, particularly for small values.
  const float cp_distance = 0.5f + std::round(3 * clamped_thickness + 0.5f);
  return {
      .wavelength = wavelength,
      .control_point_distance = cp_distance,
      // Offset the start point, so the bezier curve starts before the current
      // line, that way we can clip it exactly the same way in both ends.
      .phase = -wavelength,
  };
}

// Computes the wavy pattern rect, which is where the desired wavy pattern
// would be found when painting the wavy stroke path at the origin, or in other
// words, how far the tile needs to be translated in the opposite direction
// when painting to ensure that nothing is painted at y<0.
gfx::RectF ComputeWavyPatternRect(const float thickness,
                                  const WaveDefinition& wave,
                                  const Path& stroke_path) {
  StrokeData stroke_data;
  stroke_data.SetThickness(thickness);

  // Expand the stroke rect to integer y coordinates in both directions, to
  // avoid messing with the vertical antialiasing.
  gfx::RectF stroke_rect = stroke_path.StrokeBoundingRect(stroke_data);
  float top = floorf(stroke_rect.y());
  float bottom = ceilf(stroke_rect.bottom());
  return {0.f, top, wave.wavelength, bottom - top};
}

struct WavyParams {
  WaveDefinition wave;
  float thickness;

  bool operator==(const WavyParams&) const = default;
  DISALLOW_NEW();
};

class WavyGeometry {
  DISALLOW_NEW();

 public:
  explicit WavyGeometry(const WavyParams& params)
      : path_(WavyPath(params.wave)),
        bounds_(ComputeWavyPatternRect(params.thickness, params.wave, path_)),
        thickness_(params.thickness) {}

  gfx::RectF PaintRect(const DecorationGeometry& geometry) const;

  const cc::PaintRecord& TileRecord(const Color& color) const;
  gfx::RectF TileRect() const {
    // The wavy tile rect is the same size as the wavy pattern rect but at
    // origin (0,0).
    return gfx::RectF(bounds_.size());
  }

  // Paints the wavy ribbon as a single full-width path drawn with
  // `paint_flags`, skipping the tile shader so any `PathEffect` (e.g.
  // `stroke-dasharray`) lays out continuously across the whole decoration
  // instead of restarting at each wavelength.
  void PaintStroke(GraphicsContext& context,
                   const DecorationGeometry& geometry,
                   const cc::PaintFlags& paint_flags,
                   const AutoDarkMode& auto_dark_mode) const;

 private:
  // `PaintRect` expanded vertically by half of `stroke_width` so a stroke of
  // that width painted along the ribbon outline isn't clipped at peaks and
  // troughs.
  gfx::RectF StrokeClipRect(const DecorationGeometry& geometry,
                            float stroke_width) const;

  // Origin to translate the wavy ribbon path to so that its centerline (y=0
  // in path-local space) lands on the ribbon centerline.
  gfx::PointF StrokePathOrigin(const DecorationGeometry& geometry) const;

  Path path_;
  gfx::RectF bounds_;
  float thickness_;
  mutable cc::PaintRecord tile_record_;
  mutable Color tile_record_color_;
};

gfx::RectF WavyGeometry::PaintRect(const DecorationGeometry& geometry) const {
  // The offset from the local origin is the wavy offset and the origin of the
  // wavy pattern rect (around minus half the amplitude).
  gfx::PointF origin = geometry.line.origin() + bounds_.OffsetFromOrigin() +
                       gfx::Vector2dF{0.f, geometry.wavy_offset};
  // Get the height of the wavy tile, and the width of the decoration.
  gfx::SizeF size(geometry.line.width(), bounds_.height());
  return {origin, size};
}

gfx::RectF WavyGeometry::StrokeClipRect(const DecorationGeometry& geometry,
                                        float stroke_width) const {
  gfx::RectF rect = PaintRect(geometry);
  rect.Outset(gfx::OutsetsF::VH(stroke_width * 0.5f, 0.f));
  return rect;
}

gfx::PointF WavyGeometry::StrokePathOrigin(
    const DecorationGeometry& geometry) const {
  // Equivalent to `PaintRect(geometry).origin() - (0, bounds_.y())`: the path
  // is translated so its centerline (path-local y=0) lands on the ribbon
  // centerline, without exposing the `bounds_.y()` adjustment to the caller.
  return geometry.line.origin() +
         gfx::Vector2dF{bounds_.x(), geometry.wavy_offset};
}

const cc::PaintRecord& WavyGeometry::TileRecord(const Color& color) const {
  if (tile_record_color_ != color || tile_record_.empty()) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color.Rgb());
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(thickness_);

    PaintRecorder recorder;
    cc::PaintCanvas* canvas = recorder.beginRecording();
    // Translate the wavy pattern so that nothing is painted at y<0.
    canvas->translate(-bounds_.x(), -bounds_.y());
    canvas->drawPath(path_.GetSkPath(), flags);
    tile_record_ = recorder.finishRecordingAsPicture();
    tile_record_color_ = color;
  }
  return tile_record_;
}

void WavyGeometry::PaintStroke(GraphicsContext& context,
                               const DecorationGeometry& geometry,
                               const cc::PaintFlags& paint_flags,
                               const AutoDarkMode& auto_dark_mode) const {
  const Path centerline =
      WavyCenterlinePath(geometry.wavy_wave, geometry.line.width());

  StrokeData ribbon;
  ribbon.SetThickness(thickness_);
  const SkPath outline = centerline.StrokePath(ribbon, AffineTransform());

  // Clip horizontally to the underline width to hide the centerline's
  // one-wavelength overhang on each side. The vertical clip extends past
  // the ribbon by the SVG stroke's half-width so the stroked outline
  // boundary isn't cut off at peaks/troughs.
  const gfx::RectF clip_rect =
      StrokeClipRect(geometry, paint_flags.getStrokeWidth());
  const gfx::PointF path_origin = StrokePathOrigin(geometry);
  GraphicsContextStateSaver state_saver(context);
  context.Clip(clip_rect);
  context.Translate(path_origin.x(), path_origin.y());
  context.DrawPath(outline, paint_flags, auto_dark_mode);
}

const WavyGeometry& GetWavyGeometry(const DecorationGeometry& line_geometry) {
  struct WavyCache {
    WavyParams key;
    WavyGeometry geometry;

    DISALLOW_NEW();
  };

  DEFINE_STATIC_LOCAL(std::optional<WavyCache>, wavy_cache, (std::nullopt));

  const WavyParams params{line_geometry.wavy_wave, line_geometry.Thickness()};
  if (!wavy_cache || wavy_cache->key != params) {
    wavy_cache.emplace(params, WavyGeometry(params));
  }
  return wavy_cache->geometry;
}

}  // namespace

DecorationGeometry DecorationGeometry::Make(StrokeStyle style,
                                            const gfx::RectF& line,
                                            float double_offset,
                                            float wavy_offset,
                                            const WaveDefinition* custom_wave) {
  DecorationGeometry geometry;
  geometry.style = style;
  geometry.line = line;
  geometry.double_offset = double_offset;

  if (geometry.style == kWavyStroke) {
    geometry.wavy_wave =
        custom_wave ? *custom_wave : MakeWave(geometry.Thickness());
    geometry.wavy_offset = wavy_offset;
  }
  return geometry;
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
      return GetWavyGeometry(geometry).PaintRect(geometry);
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

void DecorationLinePainter::Paint(const DecorationGeometry& geometry,
                                  const Color& color,
                                  const AutoDarkMode& auto_dark_mode,
                                  const cc::PaintFlags* flags) {
  if (geometry.line.width() <= 0) {
    return;
  }

  switch (geometry.style) {
    case kWavyStroke:
      PaintWavyTextDecoration(geometry, color, auto_dark_mode, flags);
      break;
    case kDottedStroke:
    case kDashedStroke: {
      StyledStrokeData styled_stroke;
      styled_stroke.SetStyle(geometry.style);
      styled_stroke.SetThickness(geometry.Thickness());

      context_.SetShouldAntialias(geometry.antialias);
      context_.SetStrokeColor(color);

      DrawLineAsStroke(context_, geometry.line, styled_stroke, auto_dark_mode,
                       flags);
      break;
    }
    case kSolidStroke:
    case kDoubleStroke:
      context_.SetStrokeColor(color);

      DrawLineAsRect(context_, geometry.line, auto_dark_mode, flags);

      if (geometry.style == kDoubleStroke) {
        const gfx::RectF second_line_rect =
            geometry.line + gfx::Vector2dF(0, geometry.double_offset);
        DrawLineAsRect(context_, second_line_rect, auto_dark_mode, flags);
      }
      break;
  }
}

void DecorationLinePainter::PaintWavyTextDecoration(
    const DecorationGeometry& geometry,
    const Color& color,
    const AutoDarkMode& auto_dark_mode,
    const cc::PaintFlags* paint_flags) {
  const WavyGeometry& wavy_geometry = GetWavyGeometry(geometry);

  // SVG stroke pass: draw the full-width wavy outline with the caller's flags
  // so any `PathEffect` (e.g. dasharray) lays out across the whole decoration
  // instead of restarting per tile.
  if (paint_flags && paint_flags->getStyle() == cc::PaintFlags::kStroke_Style) {
    wavy_geometry.PaintStroke(context_, geometry, *paint_flags,
                              auto_dark_mode);
    return;
  }

  // Fill pass / non-SVG callers: cached per-color tile shader.
  const gfx::RectF paint_rect = wavy_geometry.PaintRect(geometry);
  const gfx::RectF tile_rect = wavy_geometry.TileRect();

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setShader(PaintShader::MakePaintRecord(
      wavy_geometry.TileRecord(color), gfx::RectFToSkRect(tile_rect),
      SkTileMode::kRepeat, SkTileMode::kDecal, nullptr));

  GraphicsContextStateSaver state_saver(context_);
  context_.Translate(paint_rect.x(), paint_rect.y());
  context_.DrawRect(gfx::RectFToSkRect(gfx::RectF(paint_rect.size())), flags,
                    auto_dark_mode);
}

}  // namespace blink
