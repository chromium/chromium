// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_decoration_info.h"

#include <math.h>

#include "build/build_config.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"
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
    const ComputedStyle& style) {
  const TextUnderlinePosition position = style.GetTextUnderlinePosition();

  // |auto| should resolve to |under| to avoid drawing through glyphs in
  // scripts where it would not be appropriate (e.g., ideographs.)
  // However, this has performance implications. For now, we only work with
  // vertical text.
  if (style.GetFontBaseline() != kCentralBaseline) {
    if (EnumHasFlags(position, TextUnderlinePosition::kUnder)) {
      return ResolvedUnderlinePosition::kUnder;
    }
    if (EnumHasFlags(position, TextUnderlinePosition::kFromFont)) {
      return ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont;
    }
    return ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;
  }
  // Compute language-appropriate default underline position.
  // https://drafts.csswg.org/css-text-decor-3/#default-stylesheet
  UScriptCode script = style.GetFontDescription().GetScript();
  if (script == USCRIPT_KATAKANA_OR_HIRAGANA || script == USCRIPT_HANGUL) {
    if (EnumHasFlags(position, TextUnderlinePosition::kLeft)) {
      return ResolvedUnderlinePosition::kUnder;
    }
    return ResolvedUnderlinePosition::kOver;
  }
  if (EnumHasFlags(position, TextUnderlinePosition::kRight)) {
    return ResolvedUnderlinePosition::kOver;
  }
  return ResolvedUnderlinePosition::kUnder;
}

inline bool ShouldUseDecoratingBox(const ComputedStyle& style) {
  // Disable the decorating box for styles not in the tree, because they can't
  // find the decorating box. For example, |HighlightPainter| creates a
  // |kPseudoIdHighlight| pseudo style on the fly.
  const PseudoId pseudo_id = style.StyleType();
  if (IsHighlightPseudoElement(pseudo_id))
    return false;
  return true;
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
    std::optional<float> font_underline_thickness =
        font_data->GetFontMetrics().UnderlineThickness();

    if (!font_underline_thickness)
      return auto_underline_thickness;

    return std::max(minimum_thickness, font_underline_thickness.value());
  }

  DCHECK(!text_decoration_thickness.IsFromFont());

  const Length& thickness_length = text_decoration_thickness.Thickness();
  float text_decoration_thickness_pixels =
      FloatValueForLength(thickness_length, computed_font_size);

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

struct WavyParams {
  float resolved_thickness;
  float effective_zoom;
  bool spelling_grammar;
  Color color;
  DISALLOW_NEW();
};

float WavyControlPointDistance(const WavyParams& params) {
  // Distance between decoration's axis and Bezier curve's control points. The
  // height of the curve is based on this distance. Increases the curve's height
  // as strokeThickness increases to make the curve look better.
  if (params.spelling_grammar)
    return 5 * params.effective_zoom;

  // Setting the distance to half-pixel values gives better antialiasing
  // results, particularly for small values.
  return 0.5 + roundf(3 * std::max<float>(1, params.resolved_thickness) + 0.5);
}

float WavyStep(const WavyParams& params) {
  // Increment used to form the diamond shape between start point (p1), control
  // points and end point (p2) along the axis of the decoration. Makes the curve
  // wider as strokeThickness increases to make the curve look better.
  if (params.spelling_grammar)
    return 3 * params.effective_zoom;

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

  Path result{};
  result.MoveTo(start);

  result.AddBezierCurveTo(cp1, cp2, end);
  cp1.set_x(cp1.x() + 2.f * step);
  cp2.set_x(cp2.x() + 2.f * step);
  end.set_x(end.x() + 2.f * step);
  result.AddBezierCurveTo(cp1, cp2, end);
  cp1.set_x(cp1.x() + 2.f * step);
  cp2.set_x(cp2.x() + 2.f * step);
  end.set_x(end.x() + 2.f * step);
  result.AddBezierCurveTo(cp1, cp2, end);

  return result;
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

}  // anonymous namespace

TextDecorationInfo::TextDecorationInfo(
    LineRelativeOffset local_origin,
    LayoutUnit width,
    const ComputedStyle& target_style,
    const InlinePaintContext* inline_context,
    const TextDecorationLine selection_decoration_line,
    const Color selection_decoration_color,
    const AppliedTextDecoration* decoration_override,
    const Font* font_override,
    MinimumThickness1 minimum_thickness1,
    float scaling_factor)
    : target_style_(target_style),
      inline_context_(inline_context),
      selection_decoration_line_(selection_decoration_line),
      selection_decoration_color_(selection_decoration_color),
      decoration_override_(decoration_override),
      font_override_(font_override && font_override != &target_style.GetFont()
                         ? font_override
                         : nullptr),
      local_origin_(local_origin),
      width_(width),
      target_ascent_(GetAscent(target_style, font_override)),
      scaling_factor_(scaling_factor),
      use_decorating_box_(inline_context && !decoration_override_ &&
                          !font_override_ &&
                          ShouldUseDecoratingBox(target_style)),
      minimum_thickness_is_one_(minimum_thickness1) {
  for (wtf_size_t i = 0; i < AppliedDecorationCount(); i++)
    union_all_lines_ |= AppliedDecoration(i).Lines();
  for (wtf_size_t i = 0; i < AppliedDecorationCount(); i++) {
    if (AppliedDecoration(i).Style() == ETextDecorationStyle::kDotted ||
        AppliedDecoration(i).Style() == ETextDecorationStyle::kDashed) {
      antialias_ = true;
      break;
    }
  }

  UpdateForDecorationIndex();
}

wtf_size_t TextDecorationInfo::AppliedDecorationCount() const {
  if (HasDecorationOverride())
    return 1;
  return target_style_.AppliedTextDecorations().size();
}

const AppliedTextDecoration& TextDecorationInfo::AppliedDecoration(
    wtf_size_t index) const {
  if (HasDecorationOverride())
    return *decoration_override_;
  return target_style_.AppliedTextDecorations()[index];
}

void TextDecorationInfo::SetDecorationIndex(int decoration_index) {
  DCHECK_LT(decoration_index, static_cast<int>(AppliedDecorationCount()));
  if (decoration_index_ == decoration_index)
    return;
  decoration_index_ = decoration_index;
  UpdateForDecorationIndex();
}

// Update cached properties of |this| for the |decoration_index_|.
void TextDecorationInfo::UpdateForDecorationIndex() {
  DCHECK_LT(decoration_index_, static_cast<int>(AppliedDecorationCount()));
  applied_text_decoration_ = &AppliedDecoration(decoration_index_);
  lines_ = applied_text_decoration_->Lines();
  has_underline_ = EnumHasFlags(lines_, TextDecorationLine::kUnderline);
  has_overline_ = EnumHasFlags(lines_, TextDecorationLine::kOverline);

  // Compute the |ComputedStyle| of the decorating box.
  const ComputedStyle* decorating_box_style;
  if (use_decorating_box_) {
    DCHECK(inline_context_);
    DCHECK_EQ(inline_context_->DecoratingBoxes().size(),
              AppliedDecorationCount());
    decorating_box_ = &inline_context_->DecoratingBoxes()[decoration_index_];
    decorating_box_style = &decorating_box_->Style();

    // Disable the decorating box when the baseline is central, because the
    // decorating box doesn't produce the ideal position.
    // https://drafts.csswg.org/css-text-decor-3/#:~:text=text%20is%20not%20aligned%20to%20the%20alphabetic%20baseline
    // TODO(kojii): The vertical flow in alphabetic baseline may want to use the
    // decorating box. It needs supporting the rotated coordinate system text
    // painters use when painting vertical text.
    if (!decorating_box_style->IsHorizontalWritingMode()) [[unlikely]] {
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
    original_underline_position_ =
        ResolveUnderlinePosition(*decorating_box_style);

    // text-underline-position may flip underline and overline.
    flip_underline_and_overline_ =
        original_underline_position_ == ResolvedUnderlinePosition::kOver;
  }

  if (flip_underline_and_overline_) [[unlikely]] {
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
      NOTREACHED_IN_MIGRATION();
  }

  line_data_.line = line;
  line_data_.line_offset = line_offset;
  line_data_.double_offset = double_offset;
  line_data_.wavy_offset_factor = wavy_offset_factor;

  switch (DecorationStyle()) {
    case ETextDecorationStyle::kDotted:
    case ETextDecorationStyle::kDashed:
      line_data_.stroke_path = PrepareDottedOrDashedStrokePath();
      line_data_.wavy_tile_record = cc::PaintRecord();
      break;
    case ETextDecorationStyle::kWavy:
      line_data_.stroke_path.reset();
      ComputeWavyLineData(line_data_.wavy_pattern_rect,
                          line_data_.wavy_tile_record);
      break;
    default:
      line_data_.stroke_path.reset();
      line_data_.wavy_tile_record = cc::PaintRecord();
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
  return decorating_box_paint_offset - local_origin_.line_over;
}

void TextDecorationInfo::SetUnderlineLineData(
    const TextDecorationOffset& decoration_offset) {
  DCHECK(HasUnderline());
  // Don't apply text-underline-offset to overlines. |line_offset| is zero.
  Length line_offset;
  if (flip_underline_and_overline_) [[unlikely]] {
    line_offset = Length();
  } else {
    line_offset = applied_text_decoration_->UnderlineOffset();
  }
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
    const TextDecorationOffset& decoration_offset) {
  DCHECK(HasOverline());
  // Don't apply text-underline-offset to overline.
  Length line_offset;
  FontVerticalPositionType position;
  if (flip_underline_and_overline_) [[unlikely]] {
    line_offset = applied_text_decoration_->UnderlineOffset();
    position = FontVerticalPositionType::TopOfEmHeight;
  } else {
    line_offset = Length();
    position = FontVerticalPositionType::TextTop;
  }
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
    const TextDecorationOffset& decoration_offset) {
  DCHECK(HasSpellingOrGrammerError());
  DCHECK(!HasUnderline());
  DCHECK(!HasOverline());
  DCHECK(!HasLineThrough());
  DCHECK(applied_text_decoration_);
  const int paint_underline_offset = decoration_offset.ComputeUnderlineOffset(
      FlippedUnderlinePosition(), TargetStyle().ComputedFontSize(), FontData(),
      Length(), ResolvedThickness());
  SetLineData(HasSpellingError() ? TextDecorationLine::kSpellingError
                                 : TextDecorationLine::kGrammarError,
              paint_underline_offset);
}

bool TextDecorationInfo::ShouldAntialias() const {
#if BUILDFLAG(IS_APPLE)
  if (line_data_.line == TextDecorationLine::kSpellingError ||
      line_data_.line == TextDecorationLine::kGrammarError) {
    return true;
  }
#endif
  return antialias_;
}

ETextDecorationStyle TextDecorationInfo::DecorationStyle() const {
  if (IsSpellingOrGrammarError()) {
#if BUILDFLAG(IS_APPLE)
    return ETextDecorationStyle::kDotted;
#else
    return ETextDecorationStyle::kWavy;
#endif
  }

  DCHECK(applied_text_decoration_);
  return applied_text_decoration_->Style();
}

Color TextDecorationInfo::LineColor() const {
  if (HasSpellingError()) {
    return LayoutTheme::GetTheme().PlatformSpellingMarkerUnderlineColor();
  }
  if (HasGrammarError()) {
    return LayoutTheme::GetTheme().PlatformGrammarMarkerUnderlineColor();
  }

  if (highlight_override_)
    return *highlight_override_;

  // Find the matched normal and selection |AppliedTextDecoration|
  // and use the text-decoration-color from selection when it is.
  DCHECK(applied_text_decoration_);
  if (applied_text_decoration_->Lines() == selection_decoration_line_) {
    return selection_decoration_color_;
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
#if BUILDFLAG(IS_APPLE)
    return 2.f * decorating_box_style_->EffectiveZoom();
#else
    return 1.f * decorating_box_style_->EffectiveZoom();
#endif
  }
  return ComputeUnderlineThickness(decoration.Thickness(),
                                   decorating_box_style_);
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

void TextDecorationInfo::ComputeWavyLineData(
    gfx::RectF& pattern_rect,
    cc::PaintRecord& tile_record) const {
  struct WavyCache {
    WavyParams key;
    gfx::RectF pattern_rect;
    cc::PaintRecord tile_record;
    DISALLOW_NEW();
  };

  DEFINE_STATIC_LOCAL(std::optional<WavyCache>, wavy_cache, (std::nullopt));

  if (wavy_cache && wavy_cache->key.resolved_thickness == ResolvedThickness() &&
      wavy_cache->key.effective_zoom ==
          decorating_box_style_->EffectiveZoom() &&
      wavy_cache->key.spelling_grammar == IsSpellingOrGrammarError() &&
      wavy_cache->key.color == LineColor()) {
    pattern_rect = wavy_cache->pattern_rect;
    tile_record = wavy_cache->tile_record;
    return;
  }

  WavyParams params{ResolvedThickness(), decorating_box_style_->EffectiveZoom(),
                    IsSpellingOrGrammarError(), LineColor()};
  Path stroke_path = PrepareWavyStrokePath(params);
  pattern_rect = ComputeWavyPatternRect(params, stroke_path);
  tile_record = PrepareWavyTileRecord(params, stroke_path, pattern_rect);
  wavy_cache = WavyCache{params, pattern_rect, tile_record};
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
  NOTREACHED_IN_MIGRATION();
  return gfx::RectF();
}

gfx::RectF TextDecorationInfo::BoundsForDottedOrDashed() const {
  StyledStrokeData styled_stroke;
  styled_stroke.SetThickness(roundf(ResolvedThickness()));
  styled_stroke.SetStyle(TextDecorationStyleToStrokeStyle(DecorationStyle()));
  return line_data_.stroke_path.value().StrokeBoundingRect(
      styled_stroke.ConvertToStrokeData({}));
}

// Returns the wavy bounds, which is the same size as the wavy paint rect but
// at the origin needed by the actual decoration, for the global transform.
//
// The origin is the sum of the local origin, line offset, (wavy) double offset,
// and the origin of the wavy pattern rect (around minus half the amplitude).
gfx::RectF TextDecorationInfo::BoundsForWavy() const {
  gfx::SizeF size = WavyPaintRect().size();
  gfx::PointF origin = line_data_.wavy_pattern_rect.origin();
  origin += StartPoint().OffsetFromOrigin();
  origin += gfx::Vector2dF{0.f, DoubleOffset() * line_data_.wavy_offset_factor};
  return {origin, size};
}

// Returns the wavy paint rect, which has the height of the wavy tile rect but
// the width needed by the actual decoration, for the DrawRect operation.
//
// The origin is still (0,0) so that the shader local matrix is independent of
// the origin of the decoration, allowing Skia to cache the tile. To determine
// the origin of the decoration, use Bounds().origin().
gfx::RectF TextDecorationInfo::WavyPaintRect() const {
  gfx::RectF result = WavyTileRect();
  result.set_width(width_);
  return result;
}

// Returns the wavy tile rect, which is the same size as the wavy pattern rect
// but at origin (0,0), for converting the PaintRecord to a PaintShader.
gfx::RectF TextDecorationInfo::WavyTileRect() const {
  gfx::RectF result = line_data_.wavy_pattern_rect;
  result.set_x(0.f);
  result.set_y(0.f);
  return result;
}

cc::PaintRecord TextDecorationInfo::WavyTileRecord() const {
  return line_data_.wavy_tile_record;
}

void TextDecorationInfo::SetHighlightOverrideColor(
    const std::optional<Color>& color) {
  highlight_override_ = color;
}

Path TextDecorationInfo::PrepareDottedOrDashedStrokePath() const {
  // These coordinate transforms need to match what's happening in
  // GraphicsContext's drawLineForText and drawLine.
  gfx::PointF start_point = StartPoint();
  return DecorationLinePainter::GetPathForTextLine(
      start_point, width_, ResolvedThickness(),
      TextDecorationStyleToStrokeStyle(DecorationStyle()));
}

}  // namespace blink
