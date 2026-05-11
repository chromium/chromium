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
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

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
    const UsedFont& used_font) {
  float used_font_size = used_font.UsedSize();
  const float auto_underline_thickness = used_font_size / 10.f;

  if (text_decoration_thickness.IsAuto())
    return auto_underline_thickness;

  // In principle we would not need to test for PrimaryFont() if
  // |text_decoration_thickness.Thickness()| is fixed, but a null PrimaryFont()
  // here would be a rare / error situation anyway, so practically, we can
  // early out here.
  if (!used_font.PrimaryFont()) {
    return auto_underline_thickness;
  }

  if (text_decoration_thickness.IsFromFont()) {
    return used_font.UnderlineThickness().value_or(auto_underline_thickness);
  }

  DCHECK(!text_decoration_thickness.IsFromFont());

  const Length& thickness_length = text_decoration_thickness.Thickness();
  const float text_decoration_thickness_pixels =
      FloatValueForLength(thickness_length, used_font_size);
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
    const UsedFont& target_font,
    const InlinePaintContext* inline_context,
    const TextDecorationLine selection_decoration_line,
    const Color selection_decoration_color,
    const AppliedTextDecoration* decoration_override,
    IsSvgText is_svg_text,
    float svg_resource_scaling_factor)
    : target_style_(target_style),
      inline_context_(inline_context),
      target_used_font_(target_font),
      selection_decoration_line_(selection_decoration_line),
      selection_decoration_color_(selection_decoration_color),
      decoration_override_(decoration_override),
      local_origin_(local_origin),
      width_(width),
      target_ascent_(target_font.FloatAscent()),
      svg_resource_scaling_factor_(svg_resource_scaling_factor),
      // NOTE: The use of is_svg_text here is probably problematic.
      // See LayoutSVGInlineText::ComputeNewScaledFontForStyle() for
      // a workaround that is needed due to that.
      use_decorating_box_(inline_context && !decoration_override_ &&
                          !is_svg_text && ShouldUseDecoratingBox(target_style)),
      is_svg_text_(is_svg_text) {
  for (wtf_size_t i = 0; i < AppliedDecorationCount(); ++i) {
    const auto& decoration = AppliedDecoration(i);
    union_all_lines_ |= decoration.Lines();
    if (!antialias_ && (decoration.Style() == ETextDecorationStyle::kDotted ||
                        decoration.Style() == ETextDecorationStyle::kDashed)) {
      antialias_ = true;
    }
  }
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

const ResolvedDecoration TextDecorationInfo::ResolveDecorationAt(
    wtf_size_t decoration_index) {
  DCHECK_LT(decoration_index, AppliedDecorationCount());

  ResolvedDecoration decoration(target_used_font_);
  decoration.applied_text_decoration = &AppliedDecoration(decoration_index);
  decoration.lines = decoration.applied_text_decoration->Lines();
  decoration.has_underline =
      EnumHasFlags(decoration.lines, TextDecorationLine::kUnderline);
  decoration.has_overline =
      EnumHasFlags(decoration.lines, TextDecorationLine::kOverline);

  // Compute the |ComputedStyle| of the decorating box.
  const ComputedStyle* decorating_box_style;
  const DecoratingBox* decorating_box = nullptr;
  if (use_decorating_box_) {
    DCHECK(inline_context_);
    DCHECK_EQ(inline_context_->DecoratingBoxes().size(),
              AppliedDecorationCount());
    bool disable_decorating_box;
    if (decoration_index >= inline_context_->DecoratingBoxes().size())
        [[unlikely]] {
      disable_decorating_box = true;
    } else {
      decorating_box = &inline_context_->DecoratingBoxes()[decoration_index];
      decorating_box_style = &decorating_box->Style();

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
      decorating_box = nullptr;
      decorating_box_style = &target_style_;
    }
  } else {
    DCHECK(!decorating_box);
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
    decoration.underline_position = ResolvedUnderlinePosition::kUnder;
    std::swap(decoration.has_underline, decoration.has_overline);
  } else {
    decoration.underline_position = original_underline_position_;
  }
  decoration.is_flipped_underline_and_overline = flip_underline_and_overline_;

  if (!is_svg_text_ && decorating_box) {
    decoration.used_font = decorating_box->GetUsedFont();
  } else {
    // `target_used_font_` was already copied to decoration.used_font.
  }

  decoration.effective_zoom = decorating_box_style_->EffectiveZoom();
  decoration.offset_from_decorating_box =
      decoration.HasUnderline() && decorating_box
          ? OffsetFromDecoratingBox(*decorating_box)
          : LayoutUnit();

  decoration.resolved_thickness = ComputeThickness(decoration);
  return decoration;
}

DecorationGeometry TextDecorationInfo::ComputeLineData(
    const ResolvedDecoration& decoration,
    TextDecorationLine line,
    float line_offset) const {
  const float double_offset_from_thickness =
      decoration.resolved_thickness + 1.0f;
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
      // Floor double_offset in order to avoid double-line gap to appear of
      // different size depending on position where the double line is drawn
      // because of rounding downstream in DecorationLinePainter.
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
      spelling_wave = MakeSpellingGrammarWave(decoration.effective_zoom);
    }
#elif BUILDFLAG(IS_APPLE)
    style = kDottedStroke;
    antialias = true;
#else
    style = kWavyStroke;
    spelling_wave = MakeSpellingGrammarWave(decoration.effective_zoom);
#endif
  } else {
    style = TextDecorationStyleToStrokeStyle(
        decoration.applied_text_decoration->Style());
  }

  const gfx::PointF start_point =
      gfx::PointF(local_origin_) + gfx::Vector2dF(0, line_offset);
  DecorationGeometry geometry = DecorationGeometry::Make(
      style,
      gfx::RectF(start_point,
                 gfx::SizeF(width_, decoration.resolved_thickness)),
      double_offset, wavy_offset, base::OptionalToPtr(spelling_wave));
  geometry.antialias = antialias;
  return geometry;
}

// Returns the offset of the target text/box (|local_origin_|) from the
// decorating box.
LayoutUnit TextDecorationInfo::OffsetFromDecoratingBox(
    const DecoratingBox& decorating_box) const {
  DCHECK(use_decorating_box_);
  DCHECK(inline_context_);
  // Compute the paint offset of the decorating box. The |local_origin_| is
  // already adjusted to the paint offset.
  const LayoutUnit decorating_box_paint_offset =
      decorating_box.ContentOffsetInContainer().top +
      inline_context_->PaintOffset().top;
  return decorating_box_paint_offset - local_origin_.line_over;
}

DecorationGeometry TextDecorationInfo::ComputeUnderlineLineData(
    const ResolvedDecoration& decoration,
    const TextDecorationOffset& decoration_offset) const {
  DCHECK(decoration.HasUnderline());
  // Don't apply text-underline-offset to overlines. |line_offset| is zero.
  Length line_offset;
  if (decoration.is_flipped_underline_and_overline) [[unlikely]] {
    line_offset = Length();
  } else {
    line_offset = decoration.applied_text_decoration->UnderlineOffset();
  }
  float paint_underline_offset = decoration_offset.ComputeUnderlineOffset(
      decoration.underline_position, decoration.used_font.ComputedSize(),
      decoration.used_font.PrimaryFont(), line_offset,
      decoration.resolved_thickness);
  // The offset is for the decorating box. Convert it for the target text/box.
  paint_underline_offset += decoration.offset_from_decorating_box;
  return ComputeLineData(decoration, TextDecorationLine::kUnderline,
                         paint_underline_offset);
}

DecorationGeometry TextDecorationInfo::ComputeOverlineLineData(
    const ResolvedDecoration& decoration,
    const TextDecorationOffset& decoration_offset) const {
  DCHECK(decoration.HasOverline());
  // Don't apply text-underline-offset to overline.
  Length line_offset;
  FontVerticalPositionType position;
  if (decoration.is_flipped_underline_and_overline) [[unlikely]] {
    line_offset = decoration.applied_text_decoration->UnderlineOffset();
    position = FontVerticalPositionType::TopOfEmHeight;
  } else {
    line_offset = Length();
    position = FontVerticalPositionType::TextTop;
  }
  const int paint_overline_offset =
      decoration_offset.ComputeUnderlineOffsetForUnder(
          line_offset, TargetStyle().ComputedFontSize(),
          decoration.used_font.PrimaryFont(), decoration.resolved_thickness,
          position);
  return ComputeLineData(decoration, TextDecorationLine::kOverline,
                         paint_overline_offset);
}

DecorationGeometry TextDecorationInfo::ComputeLineThroughLineData(
    const ResolvedDecoration& decoration) const {
  DCHECK(decoration.HasLineThrough());
  // For increased line thickness, the line-through decoration needs to grow
  // in both directions from its origin, subtract half the thickness to keep
  // it centered at the same origin.
  const float line_through_offset = 2 * decoration.used_font.FloatAscent() / 3 -
                                    decoration.resolved_thickness / 2;
  return ComputeLineData(decoration, TextDecorationLine::kLineThrough,
                         line_through_offset);
}

DecorationGeometry TextDecorationInfo::ComputeSpellingOrGrammarErrorLineData(
    const ResolvedDecoration& decoration,
    const TextDecorationOffset& decoration_offset) const {
  DCHECK(decoration.HasSpellingOrGrammarError());
  DCHECK(!decoration.HasUnderline());
  DCHECK(!decoration.HasOverline());
  DCHECK(!decoration.HasLineThrough());
  const int paint_underline_offset = decoration_offset.ComputeUnderlineOffset(
      decoration.underline_position, TargetStyle().ComputedFontSize(),
      decoration.used_font.PrimaryFont(), Length(),
      decoration.resolved_thickness);
  return ComputeLineData(decoration,
                         decoration.HasSpellingError()
                             ? TextDecorationLine::kSpellingError
                             : TextDecorationLine::kGrammarError,
                         paint_underline_offset);
}

Color TextDecorationInfo::LineColor(
    const ResolvedDecoration& decoration) const {
  if (decoration.HasSpellingError()) {
    return LayoutTheme::GetTheme().PlatformSpellingMarkerUnderlineColor();
  }
  if (decoration.HasGrammarError()) {
    return LayoutTheme::GetTheme().PlatformGrammarMarkerUnderlineColor();
  }

  if (highlight_override_)
    return *highlight_override_;

  // Find the matched normal and selection |AppliedTextDecoration|
  // and use the text-decoration-color from selection when it is.
  if (decoration.lines == selection_decoration_line_) {
    return selection_decoration_color_;
  }

  return decoration.applied_text_decoration->GetColor();
}

float TextDecorationInfo::ComputeThickness(
    const ResolvedDecoration& decoration) const {
  if (decoration.HasSpellingOrGrammarError()) {
    // Spelling and grammar error thickness doesn't depend on the font size.
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/434081396): Verify with UX that this is accurate.
    // This number was derived based on visual inspection of the rendered
    // lines on device.
    // Android uses 2 "display-independent-pixels". See
    // "TextAppearance.Suggestion"
    // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/res/res/values/styles.xml;l=309
    return (base::FeatureList::IsEnabled(features::kAndroidSpellcheckNativeUi))
               ? 2.5f * decoration.effective_zoom
               : 1.f * decoration.effective_zoom;
#elif BUILDFLAG(IS_APPLE)
    return 2.f * decoration.effective_zoom;
#else
    return 1.f * decoration.effective_zoom;
#endif
  }
  const float thickness = ComputeDecorationThickness(
      decoration.applied_text_decoration->Thickness(), decoration.used_font);
  return std::max(is_svg_text_ ? 0.0f : 1.0f, thickness);
}

void TextDecorationInfo::SetHighlightOverrideColor(
    const std::optional<Color>& color) {
  highlight_override_ = color;
}

}  // namespace blink
