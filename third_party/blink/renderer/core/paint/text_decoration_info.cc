// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

namespace {

static int kUndefinedDecorationIndex = -1;

static ResolvedUnderlinePosition ResolveUnderlinePosition(
    const ComputedStyle& style,
    FontBaseline baseline_type) {
  // |auto| should resolve to |under| to avoid drawing through glyphs in
  // scripts where it would not be appropriate (e.g., ideographs.)
  // However, this has performance implications. For now, we only work with
  // vertical text.
  switch (baseline_type) {
    case kAlphabeticBaseline:
      if (style.TextUnderlinePosition() & kTextUnderlinePositionUnder)
        return ResolvedUnderlinePosition::kUnder;
      if (style.TextUnderlinePosition() & kTextUnderlinePositionFromFont)
        return ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont;
      return ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;
    case kIdeographicBaseline:
      // Compute language-appropriate default underline position.
      // https://drafts.csswg.org/css-text-decor-3/#default-stylesheet
      UScriptCode script = style.GetFontDescription().GetScript();
      if (script == USCRIPT_KATAKANA_OR_HIRAGANA || script == USCRIPT_HANGUL) {
        if (style.TextUnderlinePosition() & kTextUnderlinePositionLeft) {
          return ResolvedUnderlinePosition::kUnder;
        }
        return ResolvedUnderlinePosition::kOver;
      }
      if (style.TextUnderlinePosition() & kTextUnderlinePositionRight) {
        return ResolvedUnderlinePosition::kOver;
      }
      return ResolvedUnderlinePosition::kUnder;
  }
  NOTREACHED();
  return ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;
}

static bool ShouldSetDecorationAntialias(const ComputedStyle& style) {
  for (const auto& decoration : style.AppliedTextDecorations()) {
    ETextDecorationStyle decoration_style = decoration.Style();
    if (decoration_style == ETextDecorationStyle::kDotted ||
        decoration_style == ETextDecorationStyle::kDashed)
      return true;
  }
  return false;
}

static float ComputeDecorationThickness(
    const TextDecorationThickness text_decoration_thickness,
    const ComputedStyle& style,
    const SimpleFontData* font_data) {
  float auto_underline_thickness =
      std::max(1.f, style.ComputedFontSize() / 10.f);

  if (text_decoration_thickness.IsAuto())
    return auto_underline_thickness;

  // In principle we would not need to test for font_data if
  // |text_decoration_thickness.Thickness()| is fixed, but a null font_data here
  // would be a rare / error situation anyway, so practically, we can
  // early out here.
  if (!font_data)
    return auto_underline_thickness;

  if (text_decoration_thickness.IsFromFont()) {
    base::Optional<float> underline_thickness_font_metric =
        font_data->GetFontMetrics().UnderlineThickness().value();

    if (!underline_thickness_font_metric)
      return auto_underline_thickness;

    return std::max(1.f, underline_thickness_font_metric.value());
  }

  DCHECK(!text_decoration_thickness.IsFromFont());

  const Length& thickness_length = text_decoration_thickness.Thickness();
  float font_size = font_data->PlatformData().size();
  float text_decoration_thickness_pixels =
      FloatValueForLength(thickness_length, font_size);

  return std::max(1.f, text_decoration_thickness_pixels);
}

static void AdjustStepToDecorationLength(float& step,
                                         float& control_point_distance,
                                         float length) {
  DCHECK_GT(step, 0);

  if (length <= 0)
    return;

  unsigned step_count = static_cast<unsigned>(length / step);

  // Each Bezier curve starts at the same pixel that the previous one
  // ended. We need to subtract (stepCount - 1) pixels when calculating the
  // length covered to account for that.
  float uncovered_length = length - (step_count * step - (step_count - 1));
  float adjustment = uncovered_length / step_count;
  step += adjustment;
  control_point_distance += adjustment;
}

static enum StrokeStyle TextDecorationStyleToStrokeStyle(
    ETextDecorationStyle decoration_style) {
  enum StrokeStyle stroke_style = kSolidStroke;
  switch (decoration_style) {
    case ETextDecorationStyle::kSolid:
      stroke_style = kSolidStroke;
      break;
    case ETextDecorationStyle::kDouble:
      stroke_style = kDoubleStroke;
      break;
    case ETextDecorationStyle::kDotted:
      stroke_style = kDottedStroke;
      break;
    case ETextDecorationStyle::kDashed:
      stroke_style = kDashedStroke;
      break;
    case ETextDecorationStyle::kWavy:
      stroke_style = kWavyStroke;
      break;
  }

  return stroke_style;
}

static int TextDecorationToLineDataIndex(TextDecoration line) {
  switch (line) {
    case TextDecoration::kUnderline:
      return 0;
    case TextDecoration::kOverline:
      return 1;
    case TextDecoration::kLineThrough:
      return 2;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // anonymous namespace

TextDecorationInfo::TextDecorationInfo(
    const PhysicalOffset& box_origin,
    PhysicalOffset local_origin,
    LayoutUnit width,
    FontBaseline baseline_type,
    const ComputedStyle& style,
    const base::Optional<AppliedTextDecoration> selection_text_decoration,
    const ComputedStyle* decorating_box_style)
    : style_(style),
      selection_text_decoration_(selection_text_decoration),
      baseline_type_(baseline_type),
      width_(width),
      font_data_(style_.GetFont().PrimaryFont()),
      baseline_(font_data_ ? font_data_->GetFontMetrics().FloatAscent() : 0),
      underline_position_(ResolveUnderlinePosition(style_, baseline_type_)),
      local_origin_(FloatPoint(local_origin)),
      antialias_(ShouldSetDecorationAntialias(style)),
      decoration_index_(kUndefinedDecorationIndex) {
  DCHECK(font_data_);

  for (const AppliedTextDecoration& decoration :
       style_.AppliedTextDecorations()) {
    applied_decorations_thickness_.push_back(ComputeUnderlineThickness(
        decoration.Thickness(), decorating_box_style));
  }
  DCHECK_EQ(style_.AppliedTextDecorations().size(),
            applied_decorations_thickness_.size());
}

void TextDecorationInfo::SetDecorationIndex(int decoration_index) {
  DCHECK_LT(decoration_index,
            static_cast<int>(applied_decorations_thickness_.size()));
  decoration_index_ = decoration_index;
}

void TextDecorationInfo::SetPerLineData(TextDecoration line,
                                        float line_offset,
                                        float double_offset,
                                        int wavy_offset_factor) {
  int index = TextDecorationToLineDataIndex(line);
  line_data_[index].line_offset = line_offset;
  line_data_[index].double_offset = double_offset;
  line_data_[index].wavy_offset_factor = wavy_offset_factor;
  line_data_[index].stroke_path.reset();
}

ETextDecorationStyle TextDecorationInfo::DecorationStyle() const {
  return style_.AppliedTextDecorations()[decoration_index_].Style();
}

Color TextDecorationInfo::LineColor() const {
  // Find the matched normal and selection |AppliedTextDecoration|
  // and use the text-decoration-color from selection when it is.
  if (selection_text_decoration_ &&
      style_.AppliedTextDecorations()[decoration_index_].Lines() ==
          selection_text_decoration_.value().Lines()) {
    return selection_text_decoration_.value().GetColor();
  }

  return style_.AppliedTextDecorations()[decoration_index_].GetColor();
}

FloatPoint TextDecorationInfo::StartPoint(TextDecoration line) const {
  return local_origin_ +
         FloatPoint(
             0, line_data_[TextDecorationToLineDataIndex(line)].line_offset);
}
float TextDecorationInfo::DoubleOffset(TextDecoration line) const {
  return line_data_[TextDecorationToLineDataIndex(line)].double_offset;
}

enum StrokeStyle TextDecorationInfo::StrokeStyle() const {
  return TextDecorationStyleToStrokeStyle(DecorationStyle());
}

float TextDecorationInfo::ComputeUnderlineThickness(
    const TextDecorationThickness& applied_decoration_thickness,
    const ComputedStyle* decorating_box_style) {
  float thickness = 0;
  if ((underline_position_ ==
       ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto) ||
      underline_position_ ==
          ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont) {
    thickness = ComputeDecorationThickness(applied_decoration_thickness, style_,
                                           style_.GetFont().PrimaryFont());
  } else {
    // Compute decorating box. Position and thickness are computed from the
    // decorating box.
    // Only for non-Roman for now for the performance implications.
    // https:// drafts.csswg.org/css-text-decor-3/#decorating-box
    if (decorating_box_style) {
      thickness = ComputeDecorationThickness(
          applied_decoration_thickness, *decorating_box_style,
          decorating_box_style->GetFont().PrimaryFont());
    } else {
      thickness = ComputeDecorationThickness(
          applied_decoration_thickness, style_, style_.GetFont().PrimaryFont());
    }
  }
  return thickness;
}

FloatRect TextDecorationInfo::BoundsForLine(TextDecoration line) const {
  FloatPoint start_point = StartPoint(line);
  switch (DecorationStyle()) {
    case ETextDecorationStyle::kDotted:
    case ETextDecorationStyle::kDashed:
      return BoundsForDottedOrDashed(line);
    case ETextDecorationStyle::kWavy:
      return BoundsForWavy(line);
    case ETextDecorationStyle::kDouble:
      if (DoubleOffset(line) > 0) {
        return FloatRect(start_point.X(), start_point.Y(), width_,
                         DoubleOffset(line) + ResolvedThickness());
      }
      return FloatRect(start_point.X(), start_point.Y() + DoubleOffset(line),
                       width_, -DoubleOffset(line) + ResolvedThickness());
    case ETextDecorationStyle::kSolid:
      return FloatRect(start_point.X(), start_point.Y(), width_,
                       ResolvedThickness());
    default:
      break;
  }
  NOTREACHED();
  return FloatRect();
}

FloatRect TextDecorationInfo::BoundsForDottedOrDashed(
    TextDecoration line) const {
  int line_data_index = TextDecorationToLineDataIndex(line);
  if (!line_data_[line_data_index].stroke_path) {
    // These coordinate transforms need to match what's happening in
    // GraphicsContext's drawLineForText and drawLine.
    FloatPoint start_point = StartPoint(line);
    line_data_[TextDecorationToLineDataIndex(line)].stroke_path =
        GraphicsContext::GetPathForTextLine(
            start_point, width_, ResolvedThickness(),
            TextDecorationStyleToStrokeStyle(DecorationStyle()));
  }

  StrokeData stroke_data;
  stroke_data.SetThickness(roundf(ResolvedThickness()));
  stroke_data.SetStyle(TextDecorationStyleToStrokeStyle(DecorationStyle()));
  return line_data_[line_data_index].stroke_path.value().StrokeBoundingRect(
      stroke_data);
}

FloatRect TextDecorationInfo::BoundsForWavy(TextDecoration line) const {
  StrokeData stroke_data;
  stroke_data.SetThickness(ResolvedThickness());
  return PrepareWavyStrokePath(line)->StrokeBoundingRect(stroke_data);
}

/*
 * Prepare a path for a cubic Bezier curve and repeat the same pattern long the
 * the decoration's axis.  The start point (p1), controlPoint1, controlPoint2
 * and end point (p2) of the Bezier curve form a diamond shape:
 *
 *                              step
 *                         |-----------|
 *
 *                   controlPoint1
 *                         +
 *
 *
 *                  . .
 *                .     .
 *              .         .
 * (x1, y1) p1 +           .            + p2 (x2, y2) - <--- Decoration's axis
 *                          .         .               |
 *                            .     .                 |
 *                              . .                   | controlPointDistance
 *                                                    |
 *                                                    |
 *                         +                          -
 *                   controlPoint2
 *
 *             |-----------|
 *                 step
 */
base::Optional<Path> TextDecorationInfo::PrepareWavyStrokePath(
    TextDecoration line) const {
  int line_data_index = TextDecorationToLineDataIndex(line);
  if (line_data_[line_data_index].stroke_path)
    return line_data_[line_data_index].stroke_path;

  FloatPoint start_point = StartPoint(line);
  float wave_offset =
      DoubleOffset(line) *
      line_data_[TextDecorationToLineDataIndex(line)].wavy_offset_factor;

  FloatPoint p1(start_point + FloatPoint(0, wave_offset));
  FloatPoint p2(start_point + FloatPoint(width_, wave_offset));

  GraphicsContext::AdjustLineToPixelBoundaries(p1, p2, ResolvedThickness());

  Path& path = line_data_[line_data_index].stroke_path.emplace();
  path.MoveTo(p1);

  // Distance between decoration's axis and Bezier curve's control points.
  // The height of the curve is based on this distance. Use a minimum of 6
  // pixels distance since
  // the actual curve passes approximately at half of that distance, that is 3
  // pixels.
  // The minimum height of the curve is also approximately 3 pixels. Increases
  // the curve's height
  // as strockThickness increases to make the curve looks better.
  float control_point_distance = 3 * std::max<float>(2, ResolvedThickness());

  // Increment used to form the diamond shape between start point (p1),
  // control points and end point (p2) along the axis of the decoration. Makes
  // the curve wider as strockThickness increases to make the curve looks
  // better.
  float step = 2 * std::max<float>(2, ResolvedThickness());

  bool is_vertical_line = (p1.X() == p2.X());

  if (is_vertical_line) {
    DCHECK(p1.X() == p2.X());

    float x_axis = p1.X();
    float y1;
    float y2;

    if (p1.Y() < p2.Y()) {
      y1 = p1.Y();
      y2 = p2.Y();
    } else {
      y1 = p2.Y();
      y2 = p1.Y();
    }

    AdjustStepToDecorationLength(step, control_point_distance, y2 - y1);
    FloatPoint control_point1(x_axis + control_point_distance, 0);
    FloatPoint control_point2(x_axis - control_point_distance, 0);

    for (float y = y1; y + 2 * step <= y2;) {
      control_point1.SetY(y + step);
      control_point2.SetY(y + step);
      y += 2 * step;
      path.AddBezierCurveTo(control_point1, control_point2,
                            FloatPoint(x_axis, y));
    }
  } else {
    DCHECK(p1.Y() == p2.Y());

    float y_axis = p1.Y();
    float x1;
    float x2;

    if (p1.X() < p2.X()) {
      x1 = p1.X();
      x2 = p2.X();
    } else {
      x1 = p2.X();
      x2 = p1.X();
    }

    AdjustStepToDecorationLength(step, control_point_distance, x2 - x1);
    FloatPoint control_point1(0, y_axis + control_point_distance);
    FloatPoint control_point2(0, y_axis - control_point_distance);

    for (float x = x1; x + 2 * step <= x2;) {
      control_point1.SetX(x + step);
      control_point2.SetX(x + step);
      x += 2 * step;
      path.AddBezierCurveTo(control_point1, control_point2,
                            FloatPoint(x, y_axis));
    }
  }
  return line_data_[line_data_index].stroke_path;
}

}  // namespace blink
