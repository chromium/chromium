// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_decoration_info.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

inline float GetAscent(const ComputedStyle& style, const Font* font_override) {
  const Font& font = font_override ? *font_override : style.GetFont();
  if (const SimpleFontData* primary_font = font.PrimaryFont())
    return primary_font->GetFontMetrics().FloatAscent();
  return 0.f;
}

static ResolvedUnderlinePosition ResolveUnderlinePosition(
    const ComputedStyle& style,
    const absl::optional<FontBaseline>& baseline_type_override) {
  const FontBaseline baseline_type = baseline_type_override
                                         ? *baseline_type_override
                                         : style.GetFontBaseline();

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

inline bool ShouldUseDecoratingBox(const ComputedStyle& style) {
  // Disable the decorating box for styles not in the tree, because they can't
  // find the decorating box. For example, |NGHighlightPainter| creates a
  // |kPseudoIdHighlight| pseudo style on the fly.
  const PseudoId pseudo_id = style.StyleType();
  if (pseudo_id == kPseudoIdSelection || pseudo_id == kPseudoIdTargetText ||
      pseudo_id == kPseudoIdHighlight)
    return false;
  return true;
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

  return std::max(minimum_thickness, roundf(text_decoration_thickness_pixels));
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
    const ComputedStyle& target_style,
    const NGInlinePaintContext* inline_context,
    const absl::optional<AppliedTextDecoration> selection_text_decoration,
    const Font* font_override,
    MinimumThickness1 minimum_thickness1,
    float scaling_factor,
    absl::optional<FontBaseline> baseline_type_override,
    const ComputedStyle* decorating_box_style)
    : target_style_(target_style),
      inline_context_(inline_context),
      selection_text_decoration_(selection_text_decoration),
      font_override_(font_override && font_override != &target_style.GetFont()
                         ? font_override
                         : nullptr),
      decorating_box_style_override_(decorating_box_style),
      baseline_type_override_(baseline_type_override),
      local_origin_(local_origin),
      width_(width),
      target_ascent_(GetAscent(target_style, font_override)),
      scaling_factor_(scaling_factor),
      use_decorating_box_(RuntimeEnabledFeatures::TextDecoratingBoxEnabled() &&
                          inline_context && !font_override_ &&
                          !decorating_box_style_override_ &&
                          !baseline_type_override_ &&
                          ShouldUseDecoratingBox(target_style)),
      minimum_thickness_is_one_(minimum_thickness1),
      antialias_(ShouldSetDecorationAntialias(target_style)) {
  UpdateForDecorationIndex();
}

void TextDecorationInfo::SetDecorationIndex(int decoration_index) {
  DCHECK_LT(decoration_index,
            static_cast<int>(target_style_.AppliedTextDecorations().size()));
  if (decoration_index_ == decoration_index)
    return;
  decoration_index_ = decoration_index;
  UpdateForDecorationIndex();
}

// Update cached properties of |this| for the |decoration_index_|.
void TextDecorationInfo::UpdateForDecorationIndex() {
  DCHECK_LT(decoration_index_,
            static_cast<int>(target_style_.AppliedTextDecorations().size()));
  applied_text_decoration_ =
      &target_style_.AppliedTextDecorations()[decoration_index_];
  lines_ = applied_text_decoration_->Lines();
  has_underline_ = EnumHasFlags(lines_, TextDecorationLine::kUnderline);
  has_overline_ = EnumHasFlags(lines_, TextDecorationLine::kOverline);

  // Compute the |ComputedStyle| of the decorating box.
  //
  // |decorating_box_style_override_| is intentionally ignored, as it is used
  // only by the legacy, and the legacy uses it only when computing thickness.
  // See |ComputeThickness|.
  const ComputedStyle* decorating_box_style;
  if (use_decorating_box_) {
    DCHECK(inline_context_);
    DCHECK_EQ(inline_context_->DecoratingBoxes().size(),
              target_style_.AppliedTextDecorations().size());
    decorating_box_ = &inline_context_->DecoratingBoxes()[decoration_index_];
    decorating_box_style = &decorating_box_->Style();

    // Disable the decorating box when the baseline is central, because the
    // decorating box doesn't produce the ideal position.
    // https://drafts.csswg.org/css-text-decor-3/#:~:text=text%20is%20not%20aligned%20to%20the%20alphabetic%20baseline
    // TODO(kojii): The vertical flow in alphabetic baseline may want to use the
    // decorating box. It needs supporting the rotated coordinate system text
    // painters use when painting vertical text.
    if (UNLIKELY(!decorating_box_style->IsHorizontalWritingMode())) {
      use_decorating_box_ = false;
      decorating_box_ = nullptr;
      decorating_box_style = &target_style_;
    }
  } else {
    DCHECK(!decorating_box_);
    decorating_box_style = &target_style_;
  }
  DCHECK(decorating_box_style);
  if (decorating_box_style != decorating_box_style_) {
    decorating_box_style_ = decorating_box_style;
    original_underline_position_ = ResolveUnderlinePosition(
        *decorating_box_style, baseline_type_override_);

    // text-underline-position may flip underline and overline.
    flip_underline_and_overline_ =
        original_underline_position_ == ResolvedUnderlinePosition::kOver;
  }

  if (UNLIKELY(flip_underline_and_overline_)) {
    flipped_underline_position_ = ResolvedUnderlinePosition::kUnder;
    std::swap(has_underline_, has_overline_);
  } else {
    flipped_underline_position_ = original_underline_position_;
  }

  // Compute the |Font| and its properties.
  const Font* font =
      font_override_ ? font_override_ : &decorating_box_style_->GetFont();
  DCHECK(font);
  if (font != font_) {
    font_ = font;
    computed_font_size_ = font->GetFontDescription().ComputedSize();

    const SimpleFontData* font_data = font->PrimaryFont();
    if (font_data != font_data_) {
      font_data_ = font_data;
      ascent_ = font_data ? font_data->GetFontMetrics().FloatAscent() : 0;
    }
  }

  resolved_thickness_ = ComputeThickness();
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

// Returns the offset of the target text/box (|local_origin_|) from the
// decorating box.
LayoutUnit TextDecorationInfo::OffsetFromDecoratingBox() const {
  DCHECK(use_decorating_box_);
  DCHECK(inline_context_);
  DCHECK(decorating_box_);
  // Compute the paint offset of the decorating box. The |local_origin_| is
  // already adjusted to the paint offset.
  const LayoutUnit decorating_box_paint_offset =
      decorating_box_->ContentOffsetInContainer().top +
      inline_context_->PaintOffset().top;
  return decorating_box_paint_offset - local_origin_.top;
}

void TextDecorationInfo::SetUnderlineLineData(
    const TextDecorationOffsetBase& decoration_offset) {
  DCHECK(HasUnderline());
  // Don't apply text-underline-offset to overlines. |line_offset| is zero.
  const Length line_offset = UNLIKELY(flip_underline_and_overline_)
                                 ? Length()
                                 : applied_text_decoration_->UnderlineOffset();
  float paint_underline_offset = decoration_offset.ComputeUnderlineOffset(
      FlippedUnderlinePosition(), ComputedFontSize(), FontData(), line_offset,
      ResolvedThickness());
  if (use_decorating_box_) {
    // The offset is for the decorating box. Convert it for the target text/box.
    paint_underline_offset += OffsetFromDecoratingBox();
  }
  SetLineData(TextDecorationLine::kUnderline, paint_underline_offset);
}

void TextDecorationInfo::SetOverlineLineData(
    const TextDecorationOffsetBase& decoration_offset) {
  DCHECK(HasOverline());
  // Don't apply text-underline-offset to overline.
  const Length line_offset = UNLIKELY(flip_underline_and_overline_)
                                 ? applied_text_decoration_->UnderlineOffset()
                                 : Length();
  const FontVerticalPositionType position =
      UNLIKELY(flip_underline_and_overline_)
          ? FontVerticalPositionType::TopOfEmHeight
          : FontVerticalPositionType::TextTop;
  const int paint_overline_offset =
      decoration_offset.ComputeUnderlineOffsetForUnder(
          line_offset, TargetStyle().ComputedFontSize(), FontData(),
          ResolvedThickness(), position);
  SetLineData(TextDecorationLine::kOverline, paint_overline_offset);
}

void TextDecorationInfo::SetLineThroughLineData() {
  DCHECK(HasLineThrough());
  // For increased line thickness, the line-through decoration needs to grow
  // in both directions from its origin, subtract half the thickness to keep
  // it centered at the same origin.
  const float line_through_offset = 2 * Ascent() / 3 - ResolvedThickness() / 2;
  SetLineData(TextDecorationLine::kLineThrough, line_through_offset);
}

void TextDecorationInfo::SetSpellingOrGrammarErrorLineData(
    const TextDecorationOffsetBase& decoration_offset) {
  DCHECK(HasSpellingOrGrammerError());
  DCHECK(!HasUnderline());
  DCHECK(!HasOverline());
  DCHECK(!HasLineThrough());
  DCHECK(applied_text_decoration_);
  const int paint_underline_offset = decoration_offset.ComputeUnderlineOffset(
      FlippedUnderlinePosition(), TargetStyle().ComputedFontSize(), FontData(),
      applied_text_decoration_->UnderlineOffset(), ResolvedThickness());
  SetLineData(HasSpellingError() ? TextDecorationLine::kSpellingError
                                 : TextDecorationLine::kGrammarError,
              paint_underline_offset);
}

ETextDecorationStyle TextDecorationInfo::DecorationStyle() const {
  if (IsSpellingOrGrammarError()) {
#if BUILDFLAG(IS_MAC)
    return ETextDecorationStyle::kDotted;
#else
    return ETextDecorationStyle::kWavy;
#endif
  }

  DCHECK(applied_text_decoration_);
  return applied_text_decoration_->Style();
}

Color TextDecorationInfo::LineColor() const {
  // TODO(rego): Allow customize the spelling and grammar error color with
  // text-decoration-color property.
  if (line_data_.line == TextDecorationLine::kSpellingError)
    return LayoutTheme::GetTheme().PlatformSpellingMarkerUnderlineColor();
  if (line_data_.line == TextDecorationLine::kGrammarError)
    return LayoutTheme::GetTheme().PlatformGrammarMarkerUnderlineColor();
  if (highlight_override_)
    return *highlight_override_;

  // Find the matched normal and selection |AppliedTextDecoration|
  // and use the text-decoration-color from selection when it is.
  DCHECK(applied_text_decoration_);
  if (selection_text_decoration_ &&
      applied_text_decoration_->Lines() ==
          selection_text_decoration_.value().Lines()) {
    return selection_text_decoration_.value().GetColor();
  }

  return applied_text_decoration_->GetColor();
}

gfx::PointF TextDecorationInfo::StartPoint() const {
  return gfx::PointF(local_origin_) + gfx::Vector2dF(0, line_data_.line_offset);
}
float TextDecorationInfo::DoubleOffset() const {
  return line_data_.double_offset;
}

enum StrokeStyle TextDecorationInfo::StrokeStyle() const {
  return TextDecorationStyleToStrokeStyle(DecorationStyle());
}

float TextDecorationInfo::ComputeThickness() const {
  DCHECK(applied_text_decoration_);
  const AppliedTextDecoration& decoration = *applied_text_decoration_;
  if (HasSpellingOrGrammerError()) {
    // Spelling and grammar error thickness doesn't depend on the font size.
#if BUILDFLAG(IS_MAC)
    return 2.f;
#else
    return 1.f;
#endif
  }

  // Use |decorating_box_style_override_| to compute thickness. It is used only
  // by the legacy, and this matches the legacy behavior.
  return ComputeUnderlineThickness(decoration.Thickness(),
                                   decorating_box_style_override_
                                       ? decorating_box_style_override_
                                       : decorating_box_style_);
}

float TextDecorationInfo::ComputeUnderlineThickness(
    const TextDecorationThickness& applied_decoration_thickness,
    const ComputedStyle* decorating_box_style) const {
  const float minimum_thickness = minimum_thickness_is_one_ ? 1.0f : 0.0f;
  float thickness = 0;
  if (flipped_underline_position_ ==
          ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto ||
      flipped_underline_position_ ==
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

void TextDecorationInfo::SetHighlightOverrideColor(
    const absl::optional<Color>& color) {
  highlight_override_ = color;
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
