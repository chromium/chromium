// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
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
  if (baseline_type != kCentralBaseline) {
    if (style.TextUnderlinePosition() & kTextUnderlinePositionUnder)
      return ResolvedUnderlinePosition::kUnder;
    if (style.TextUnderlinePosition() & kTextUnderlinePositionFromFont)
      return ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont;
    return ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;
  }
  // Compute language-appropriate default underline position.
  // https://drafts.csswg.org/css-text-decor-3/#default-stylesheet
  UScriptCode script = style.GetFontDescription().GetScript();
  if (script == USCRIPT_KATAKANA_OR_HIRAGANA || script == USCRIPT_HANGUL) {
    if (style.TextUnderlinePosition() & kTextUnderlinePositionLeft)
      return ResolvedUnderlinePosition::kUnder;
    return ResolvedUnderlinePosition::kOver;
  }
  if (style.TextUnderlinePosition() & kTextUnderlinePositionRight)
    return ResolvedUnderlinePosition::kOver;
  return ResolvedUnderlinePosition::kUnder;
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
    float computed_font_size,
    float minimum_thickness,
    const SimpleFontData* font_data) {
  float auto_underline_thickness =
      std::max(minimum_thickness, computed_font_size / 10.f);

  if (text_decoration_thickness.IsAuto())
    return auto_underline_thickness;

  // In principle we would not need to test for font_data if
  // |text_decoration_thickness.Thickness()| is fixed, but a null font_data here
  // would be a rare / error situation anyway, so practically, we can
  // early out here.
  if (!font_data)
    return auto_underline_thickness;

  if (text_decoration_thickness.IsFromFont()) {
    absl::optional<float> font_underline_thickness =
        font_data->GetFontMetrics().UnderlineThickness();

    if (!font_underline_thickness)
      return auto_underline_thickness;

    return std::max(minimum_thickness, font_underline_thickness.value());
  }

  DCHECK(!text_decoration_thickness.IsFromFont());

  const Length& thickness_length = text_decoration_thickness.Thickness();
  float font_size = font_data->PlatformData().size();
  float text_decoration_thickness_pixels =
      FloatValueForLength(thickness_length, font_size);

  return std::max(minimum_thickness, text_decoration_thickness_pixels);
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

}  // anonymous namespace

TextDecorationInfo::TextDecorationInfo(
    PhysicalOffset local_origin,
    LayoutUnit width,
    FontBaseline baseline_type,
    const ComputedStyle& style,
    const Font& scaled_font,
    const absl::optional<AppliedTextDecoration> selection_text_decoration,
    const ComputedStyle* decorating_box_style,
    MinimumThickness1 minimum_thickness1,
    float scaling_factor)
    : style_(style),
      selection_text_decoration_(selection_text_decoration),
      baseline_type_(baseline_type),
      width_(width),
      font_data_(scaled_font.PrimaryFont()),
      baseline_(font_data_ ? font_data_->GetFontMetrics().FloatAscent() : 0),
      computed_font_size_(scaled_font.GetFontDescription().ComputedSize()),
      scaling_factor_(scaling_factor),
      underline_position_(ResolveUnderlinePosition(style_, baseline_type_)),
      local_origin_(gfx::PointF(local_origin)),
      minimum_thickness_is_one_(minimum_thickness1),
      antialias_(ShouldSetDecorationAntialias(style)),
      decoration_index_(kUndefinedDecorationIndex) {
  DCHECK(font_data_);

  for (const AppliedTextDecoration& decoration :
       style_.AppliedTextDecorations()) {
    if (EnumHasFlags(decoration.Lines(), TextDecorationLine::kSpellingError) ||
        EnumHasFlags(decoration.Lines(), TextDecorationLine::kGrammarError)) {
      // Spelling and grammar error thickness doesn't depend on the font size.
#if BUILDFLAG(IS_MAC)
      applied_decorations_thickness_.push_back(2.f);
#else
      applied_decorations_thickness_.push_back(1.f);
#endif
    } else {
      applied_decorations_thickness_.push_back(ComputeUnderlineThickness(
          decoration.Thickness(), decorating_box_style));
    }
  }
  DCHECK_EQ(style_.AppliedTextDecorations().size(),
            applied_decorations_thickness_.size());
}

void TextDecorationInfo::SetDecorationIndex(int decoration_index) {
  DCHECK_LT(decoration_index,
            static_cast<int>(applied_decorations_thickness_.size()));
  decoration_index_ = decoration_index;
}

void TextDecorationInfo::SetLineData(TextDecorationLine line,
                                     float line_offset) {
  const float double_offset_from_thickness = ResolvedThickness() + 1.0f;
  float double_offset;
  int wavy_offset_factor;
  switch (line) {
    case TextDecorationLine::kUnderline:
    case TextDecorationLine::kSpellingError:
    case TextDecorationLine::kGrammarError:
      double_offset = double_offset_from_thickness;
      wavy_offset_factor = 1;
      break;
    case TextDecorationLine::kOverline:
      double_offset = -double_offset_from_thickness;
      wavy_offset_factor = 1;
      break;
    case TextDecorationLine::kLineThrough:
      // Floor double_offset in order to avoid double-line gap to appear
      // of different size depending on position where the double line
      // is drawn because of rounding downstream in
      // GraphicsContext::DrawLineForText.
      double_offset = floorf(double_offset_from_thickness);
      wavy_offset_factor = 0;
      break;
    default:
      double_offset = 0.0f;
      wavy_offset_factor = 0;
      NOTREACHED();
  }

  line_data_.line = line;
  line_data_.line_offset = line_offset;
  line_data_.double_offset = double_offset;
  line_data_.wavy_offset_factor = wavy_offset_factor;

  switch (DecorationStyle()) {
    case ETextDecorationStyle::kDotted:
    case ETextDecorationStyle::kDashed:
      line_data_.stroke_path = PrepareDottedOrDashedStrokePath();
      break;
    case ETextDecorationStyle::kWavy:
      line_data_.stroke_path = PrepareWavyStrokePath();
      break;
    default:
      line_data_.stroke_path.reset();
  }
}

ETextDecorationStyle TextDecorationInfo::DecorationStyle() const {
  if (IsSpellingOrGrammarError()) {
#if BUILDFLAG(IS_MAC)
    return ETextDecorationStyle::kDotted;
#else
    return ETextDecorationStyle::kWavy;
#endif
  }

  return style_.AppliedTextDecorations()[decoration_index_].Style();
}

Color TextDecorationInfo::LineColor() const {
  // TODO(rego): Allow customize the spelling and grammar error color with
  // text-decoration-color property.
  if (line_data_.line == TextDecorationLine::kSpellingError)
    return LayoutTheme::GetTheme().PlatformSpellingMarkerUnderlineColor();
  if (line_data_.line == TextDecorationLine::kGrammarError)
    return LayoutTheme::GetTheme().PlatformGrammarMarkerUnderlineColor();

  // Find the matched normal and selection |AppliedTextDecoration|
  // and use the text-decoration-color from selection when it is.
  if (selection_text_decoration_ &&
      style_.AppliedTextDecorations()[decoration_index_].Lines() ==
          selection_text_decoration_.value().Lines()) {
    return selection_text_decoration_.value().GetColor();
  }

  return style_.AppliedTextDecorations()[decoration_index_].GetColor();
}

gfx::PointF TextDecorationInfo::StartPoint() const {
  return local_origin_ + gfx::Vector2dF(0, line_data_.line_offset);
}
float TextDecorationInfo::DoubleOffset() const {
  return line_data_.double_offset;
}

enum StrokeStyle TextDecorationInfo::StrokeStyle() const {
  return TextDecorationStyleToStrokeStyle(DecorationStyle());
}

float TextDecorationInfo::ComputeUnderlineThickness(
    const TextDecorationThickness& applied_decoration_thickness,
    const ComputedStyle* decorating_box_style) {
  const float minimum_thickness = minimum_thickness_is_one_ ? 1.0f : 0.0f;
  float thickness = 0;
  if ((underline_position_ ==
       ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto) ||
      underline_position_ ==
          ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont) {
    thickness = ComputeDecorationThickness(applied_decoration_thickness,
                                           computed_font_size_,
                                           minimum_thickness, font_data_);
  } else {
    // Compute decorating box. Position and thickness are computed from the
    // decorating box.
    // Only for non-Roman for now for the performance implications.
    // https:// drafts.csswg.org/css-text-decor-3/#decorating-box
    if (decorating_box_style) {
      thickness = ComputeDecorationThickness(
          applied_decoration_thickness,
          decorating_box_style->ComputedFontSize(), minimum_thickness,
          decorating_box_style->GetFont().PrimaryFont());
    } else {
      thickness = ComputeDecorationThickness(applied_decoration_thickness,
                                             computed_font_size_,
                                             minimum_thickness, font_data_);
    }
  }
  return thickness;
}

gfx::RectF TextDecorationInfo::Bounds() const {
  gfx::PointF start_point = StartPoint();
  switch (DecorationStyle()) {
    case ETextDecorationStyle::kDotted:
    case ETextDecorationStyle::kDashed:
      return BoundsForDottedOrDashed();
    case ETextDecorationStyle::kWavy:
      return BoundsForWavy();
    case ETextDecorationStyle::kDouble:
      if (DoubleOffset() > 0) {
        return gfx::RectF(start_point.x(), start_point.y(), width_,
                          DoubleOffset() + ResolvedThickness());
      }
      return gfx::RectF(start_point.x(), start_point.y() + DoubleOffset(),
                        width_, -DoubleOffset() + ResolvedThickness());
    case ETextDecorationStyle::kSolid:
      return gfx::RectF(start_point.x(), start_point.y(), width_,
                        ResolvedThickness());
    default:
      break;
  }
  NOTREACHED();
  return gfx::RectF();
}

gfx::RectF TextDecorationInfo::BoundsForDottedOrDashed() const {
  StrokeData stroke_data;
  stroke_data.SetThickness(roundf(ResolvedThickness()));
  stroke_data.SetStyle(TextDecorationStyleToStrokeStyle(DecorationStyle()));
  return line_data_.stroke_path.value().StrokeBoundingRect(stroke_data);
}

gfx::RectF TextDecorationInfo::BoundsForWavy() const {
  StrokeData stroke_data;
  stroke_data.SetThickness(ResolvedThickness());
  auto bounding_rect = line_data_.stroke_path->StrokeBoundingRect(stroke_data);

  bounding_rect.set_x(StartPoint().x());
  bounding_rect.set_width(width_);
  return bounding_rect;
}

absl::optional<Path> TextDecorationInfo::StrokePath() const {
  return line_data_.stroke_path;
}

float TextDecorationInfo::WavyDecorationSizing() const {
  // Minimum unit we use to compute control point distance and step to define
  // the path of the Bezier curve.
  return std::max<float>(2, ResolvedThickness());
}

float TextDecorationInfo::ControlPointDistanceFromResolvedThickness() const {
  // Distance between decoration's axis and Bezier curve's control points. The
  // height of the curve is based on this distance. Increases the curve's height
  // as strokeThickness increases to make the curve look better.
  if (IsSpellingOrGrammarError())
    return 5;

  return 3.5 * WavyDecorationSizing();
}

float TextDecorationInfo::StepFromResolvedThickness() const {
  // Increment used to form the diamond shape between start point (p1), control
  // points and end point (p2) along the axis of the decoration. Makes the curve
  // wider as strokeThickness increases to make the curve look better.
  if (IsSpellingOrGrammarError())
    return 3;

  return 2.5 * WavyDecorationSizing();
}

Path TextDecorationInfo::PrepareDottedOrDashedStrokePath() const {
  // These coordinate transforms need to match what's happening in
  // GraphicsContext's drawLineForText and drawLine.
  gfx::PointF start_point = StartPoint();
  return GraphicsContext::GetPathForTextLine(
      start_point, width_, ResolvedThickness(),
      TextDecorationStyleToStrokeStyle(DecorationStyle()));
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
Path TextDecorationInfo::PrepareWavyStrokePath() const {
  float wave_offset = DoubleOffset() * line_data_.wavy_offset_factor;

  float control_point_distance = ControlPointDistanceFromResolvedThickness();
  // For spelling and grammar errors we invert the control_point_distance to get
  // a result closer to Microsoft Word circa 2021.
  if (IsSpellingOrGrammarError())
    control_point_distance = -control_point_distance;
  float step = StepFromResolvedThickness();

  gfx::PointF start_point = StartPoint();
  // We paint the wave before and after the text line (to cover the whole length
  // of the line) and then we clip it at
  // AppliedDecorationPainter::StrokeWavyTextDecoration().
  // Offset the start point, so the beizer curve starts before the current line,
  // that way we can clip it exactly the same way in both ends.
  // For spelling and grammar errors we offset an extra half step, to get a
  // result closer to Microsoft Word circa 2021.
  float start_offset = (IsSpellingOrGrammarError() ? -2.5 : -2) * step;
  gfx::PointF p1(start_point + gfx::Vector2dF(start_offset, wave_offset));
  // Increase the width including the previous offset, plus an extra wave to be
  // painted after the line.
  float extra_width = (IsSpellingOrGrammarError() ? 4.5 : 4) * step;
  gfx::PointF p2(start_point +
                 gfx::Vector2dF(width_ + extra_width, wave_offset));

  GraphicsContext::AdjustLineToPixelBoundaries(p1, p2, ResolvedThickness());

  Path path;
  path.MoveTo(p1);

  bool is_vertical_line = (p1.x() == p2.x());

  if (is_vertical_line) {
    DCHECK(p1.x() == p2.x());

    float x_axis = p1.x();
    float y1;
    float y2;

    if (p1.y() < p2.y()) {
      y1 = p1.y();
      y2 = p2.y();
    } else {
      y1 = p2.y();
      y2 = p1.y();
    }

    gfx::PointF control_point1(x_axis + control_point_distance, 0);
    gfx::PointF control_point2(x_axis - control_point_distance, 0);

    for (float y = y1; y + 2 * step <= y2;) {
      control_point1.set_y(y + step);
      control_point2.set_y(y + step);
      y += 2 * step;
      path.AddBezierCurveTo(control_point1, control_point2,
                            gfx::PointF(x_axis, y));
    }
  } else {
    DCHECK(p1.y() == p2.y());

    float y_axis = p1.y();
    float x1;
    float x2;

    if (p1.x() < p2.x()) {
      x1 = p1.x();
      x2 = p2.x();
    } else {
      x1 = p2.x();
      x2 = p1.x();
    }

    gfx::PointF control_point1(0, y_axis + control_point_distance);
    gfx::PointF control_point2(0, y_axis - control_point_distance);

    for (float x = x1; x + 2 * step <= x2;) {
      control_point1.set_x(x + step);
      control_point2.set_x(x + step);
      x += 2 * step;
      path.AddBezierCurveTo(control_point1, control_point2,
                            gfx::PointF(x, y_axis));
    }
  }
  return path;
}

}  // namespace blink
