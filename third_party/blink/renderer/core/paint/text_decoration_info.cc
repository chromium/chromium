// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_decoration_info.h"

#include <math.h>

#include "base/feature_list.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/decoration_line_painter.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

inline float GetAscent(const ComputedStyle& style, const Font* font_override) {
  const Font* font = font_override ? font_override : style.GetFont();
  if (const SimpleFontData* primary_font = font->PrimaryFont()) {
    return primary_font->GetFontMetrics().FloatAscent();
  }
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

float ComputeDecorationThickness(
    const TextDecorationThickness& text_decoration_thickness,
    float computed_font_size,
    const SimpleFontData* font_data) {
  const float auto_underline_thickness = computed_font_size / 10.f;

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
    return font_underline_thickness.value_or(auto_underline_thickness);
  }

  DCHECK(!text_decoration_thickness.IsFromFont());

  const Length& thickness_length = text_decoration_thickness.Thickness();
  const float text_decoration_thickness_pixels =
      FloatValueForLength(thickness_length, computed_font_size);
  return roundf(text_decoration_thickness_pixels);
}

static enum StrokeStyle TextDecorationStyleToStrokeStyle(
    ETextDecorationStyle decoration_style) {
  switch (decoration_style) {
    case ETextDecorationStyle::kSolid:
      return kSolidStroke;
    case ETextDecorationStyle::kDouble:
      return kDoubleStroke;
    case ETextDecorationStyle::kDotted:
      return kDottedStroke;
    case ETextDecorationStyle::kDashed:
      return kDashedStroke;
    case ETextDecorationStyle::kWavy:
      return kWavyStroke;
  }
}

#if !BUILDFLAG(IS_APPLE)
WaveDefinition MakeSpellingGrammarWave(float effective_zoom) {
  const float wavelength = 6 * effective_zoom;
  return {
      .wavelength = wavelength,
      .control_point_distance = 5 * effective_zoom,
      // Offset by a quarter of a wavelength, to get a result closer to
      // Microsoft Word circa 2021.
      .phase = -0.75f * wavelength,
  };
}
#endif

}  // namespace

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
      font_override_(font_override && font_override != target_style.GetFont()
                         ? font_override
                         : nullptr),
      local_origin_(local_origin),
      width_(width),
      target_ascent_(GetAscent(target_style, font_override)),
      scaling_factor_(scaling_factor),
      // NOTE: The use of font_override_ here is probably problematic.
      // See LayoutSVGInlineText::ComputeNewScaledFontForStyle() for
      // a workaround that is needed due to that.
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
    bool disable_decorating_box;
    if (static_cast<wtf_size_t>(decoration_index_) >=
        inline_context_->DecoratingBoxes().size()) [[unlikely]] {
      disable_decorating_box = true;
    } else {
      decorating_box_ = &inline_context_->DecoratingBoxes()[decoration_index_];
      decorating_box_style = &decorating_box_->Style();

      // Disable the decorating box when the baseline is central, because the
      // decorating box doesn't produce the ideal position.
      // https://drafts.csswg.org/css-text-decor-3/#:~:text=text%20is%20not%20aligned%20to%20the%20alphabetic%20baseline
      // TODO(kojii): The vertical flow in alphabetic baseline may want to use
      // the decorating box. It needs supporting the rotated coordinate system
      // text painters use when painting vertical text.
      disable_decorating_box = !decorating_box_style->IsHorizontalWritingMode();
    }

    if (disable_decorating_box) [[unlikely]] {
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
      font_override_ ? font_override_ : decorating_box_style_->GetFont();
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
  float wavy_offset;
  switch (line) {
    case TextDecorationLine::kUnderline:
    case TextDecorationLine::kSpellingError:
    case TextDecorationLine::kGrammarError:
      double_offset = double_offset_from_thickness;
      wavy_offset = double_offset_from_thickness;
      break;
    case TextDecorationLine::kOverline:
      double_offset = -double_offset_from_thickness;
      wavy_offset = -double_offset_from_thickness;
      break;
    case TextDecorationLine::kLineThrough:
      // Floor double_offset in order to avoid double-line gap to appear
      // of different size depending on position where the double line
      // is drawn because of rounding downstream in
      // GraphicsContext::DrawLineForText.
      double_offset = floorf(double_offset_from_thickness);
      wavy_offset = 0;
      break;
    case TextDecorationLine::kNone:
    case TextDecorationLine::kBlink:
      NOTREACHED();
  }

  StrokeStyle style;
  std::optional<WaveDefinition> spelling_wave;
  bool antialias = antialias_;
  if (line == TextDecorationLine::kSpellingError ||
      line == TextDecorationLine::kGrammarError) {
#if BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(features::kAndroidSpellcheckNativeUi)) {
      style = kSolidStroke;
      antialias = true;
      spelling_wave = std::nullopt;
    } else {
      style = kWavyStroke;
      spelling_wave =
          MakeSpellingGrammarWave(decorating_box_style_->EffectiveZoom());
    }
#elif BUILDFLAG(IS_APPLE)
    style = kDottedStroke;
    antialias = true;
#else
    style = kWavyStroke;
    spelling_wave =
        MakeSpellingGrammarWave(decorating_box_style_->EffectiveZoom());
#endif
  } else {
    DCHECK(applied_text_decoration_);
    style = TextDecorationStyleToStrokeStyle(applied_text_decoration_->Style());
  }

  const gfx::PointF start_point =
      gfx::PointF(local_origin_) + gfx::Vector2dF(0, line_offset);
  line_geometry_ = DecorationGeometry::Make(
      style, gfx::RectF(start_point, gfx::SizeF(width_, ResolvedThickness())),
      double_offset, wavy_offset, base::OptionalToPtr(spelling_wave));
  line_geometry_.antialias = antialias;
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
  DCHECK(HasSpellingOrGrammarError());
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

float TextDecorationInfo::ComputeThickness() const {
  DCHECK(applied_text_decoration_);
  const AppliedTextDecoration& decoration = *applied_text_decoration_;
  if (HasSpellingOrGrammarError()) {
    // Spelling and grammar error thickness doesn't depend on the font size.
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/434081396): Verify with UX that this is accurate.
    // This number was derived based on visual inspection of the rendered
    // lines on device.
    // Android uses 2 "display-independent-pixels". See
    // "TextAppearance.Suggestion"
    // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/res/res/values/styles.xml;l=309
    return (base::FeatureList::IsEnabled(features::kAndroidSpellcheckNativeUi))
               ? 2.5f * decorating_box_style_->EffectiveZoom()
               : 1.f * decorating_box_style_->EffectiveZoom();
#elif BUILDFLAG(IS_APPLE)
    return 2.f * decorating_box_style_->EffectiveZoom();
#else
    return 1.f * decorating_box_style_->EffectiveZoom();
#endif
  }
  const float thickness = ComputeDecorationThickness(
      decoration.Thickness(), computed_font_size_, font_data_);
  const float minimum_thickness = minimum_thickness_is_one_ ? 1.0f : 0.0f;
  return std::max(minimum_thickness, thickness);
}

gfx::RectF TextDecorationInfo::Bounds() const {
  return DecorationLinePainter::Bounds(GetGeometry());
}

void TextDecorationInfo::SetHighlightOverrideColor(
    const std::optional<Color>& color) {
  highlight_override_ = color;
}

}  // namespace blink
