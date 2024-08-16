// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_color.h"

#include <memory>

#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace blink {

namespace {

using UnderlyingColorType = StyleColor::UnresolvedColorMix::UnderlyingColorType;

UnderlyingColorType ResolveColorOperandType(const StyleColor& c) {
  if (c.IsUnresolvedColorMixFunction()) {
    return UnderlyingColorType::kColorMix;
  }
  if (c.IsCurrentColor()) {
    return UnderlyingColorType::kCurrentColor;
  }
  return UnderlyingColorType::kColor;
}

Color ResolveColorOperand(const StyleColor::ColorOrUnresolvedColorMix& color,
                          UnderlyingColorType type,
                          const Color& current_color) {
  switch (type) {
    case UnderlyingColorType::kColorMix:
      return color.unresolved_color_mix->Resolve(current_color);
    case UnderlyingColorType::kCurrentColor:
      return current_color;
    case UnderlyingColorType::kColor:
      return color.color;
  }
}

}  // namespace

StyleColor::UnresolvedColorMix::UnresolvedColorMix(
    Color::ColorSpace color_interpolation_space,
    Color::HueInterpolationMethod hue_interpolation_method,
    const StyleColor& c1,
    const StyleColor& c2,
    double percentage,
    double alpha_multiplier)
    : color_interpolation_space_(color_interpolation_space),
      hue_interpolation_method_(hue_interpolation_method),
      color1_(c1.color_or_unresolved_color_mix_),
      color2_(c2.color_or_unresolved_color_mix_),
      percentage_(percentage),
      alpha_multiplier_(alpha_multiplier),
      color1_type_(ResolveColorOperandType(c1)),
      color2_type_(ResolveColorOperandType(c2)) {}

Color StyleColor::UnresolvedColorMix::Resolve(
    const Color& current_color) const {
  const Color c1 = ResolveColorOperand(color1_, color1_type_, current_color);
  const Color c2 = ResolveColorOperand(color2_, color2_type_, current_color);
  return Color::FromColorMix(color_interpolation_space_,
                             hue_interpolation_method_, c1, c2, percentage_,
                             alpha_multiplier_);
}

cssvalue::CSSColorMixValue* StyleColor::UnresolvedColorMix::ToCSSColorMixValue()
    const {
  auto to_css_value = [](const ColorOrUnresolvedColorMix& color_or_mix,
                         UnderlyingColorType type) -> const CSSValue* {
    switch (type) {
      case UnderlyingColorType::kColor:
        return cssvalue::CSSColor::Create(color_or_mix.color);
      case UnderlyingColorType::kColorMix:
        CHECK(color_or_mix.unresolved_color_mix);
        return color_or_mix.unresolved_color_mix->ToCSSColorMixValue();
      case UnderlyingColorType::kCurrentColor:
        return CSSIdentifierValue::Create(CSSValueID::kCurrentcolor);
    }
  };

  const CSSPrimitiveValue* percent1 = CSSNumericLiteralValue::Create(
      100 * (1.0 - percentage_) * alpha_multiplier_,
      CSSPrimitiveValue::UnitType::kPercentage);
  const CSSPrimitiveValue* percent2 =
      CSSNumericLiteralValue::Create(100 * percentage_ * alpha_multiplier_,
                                     CSSPrimitiveValue::UnitType::kPercentage);

  return MakeGarbageCollected<cssvalue::CSSColorMixValue>(
      to_css_value(color1_, color1_type_), to_css_value(color2_, color2_type_),
      percent1, percent2, color_interpolation_space_,
      hue_interpolation_method_);
}

void StyleColor::ColorOrUnresolvedColorMix::Trace(Visitor* visitor) const {
  visitor->Trace(unresolved_color_mix);
}

Color StyleColor::Resolve(const Color& current_color,
                          mojom::blink::ColorScheme color_scheme,
                          bool* is_current_color) const {
  if (IsUnresolvedColorMixFunction()) {
    return color_or_unresolved_color_mix_.unresolved_color_mix->Resolve(
        current_color);
  }

  if (is_current_color) {
    *is_current_color = IsCurrentColor();
  }
  if (IsCurrentColor()) {
    return current_color;
  }
  if (EffectiveColorKeyword() != CSSValueID::kInvalid) {
    // It is okay to pass nullptr for color_provider here because system colors
    // are now resolved before used value time.
    CHECK(!IsSystemColorIncludingDeprecated());
    return ColorFromKeyword(color_keyword_, color_scheme,
                            /*color_provider=*/nullptr,
                            /*is_in_web_app_scope=*/false);
  }
  return GetColor();
}

Color StyleColor::ResolveWithAlpha(Color current_color,
                                   mojom::blink::ColorScheme color_scheme,
                                   int alpha,
                                   bool* is_current_color) const {
  Color color = Resolve(current_color, color_scheme, is_current_color);
  // TODO(crbug.com/1333988) This looks unfriendly to CSS Color 4.
  return Color(color.Red(), color.Green(), color.Blue(), alpha);
}

StyleColor StyleColor::ResolveSystemColor(
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider,
    bool is_in_web_app_scope) const {
  CHECK(IsSystemColor());
  Color color = ColorFromKeyword(color_keyword_, color_scheme, color_provider,
                                 is_in_web_app_scope);
  return StyleColor(color, color_keyword_);
}

Color StyleColor::ColorFromKeyword(CSSValueID keyword,
                                   mojom::blink::ColorScheme color_scheme,
                                   const ui::ColorProvider* color_provider,
                                   bool is_in_web_app_scope) {
  if (const char* value_name = getValueName(keyword)) {
    if (const NamedColor* named_color = FindColor(
            value_name, static_cast<wtf_size_t>(strlen(value_name)))) {
      return Color::FromRGBA32(named_color->argb_value);
    }
  }

  return LayoutTheme::GetTheme().SystemColor(
      keyword, color_scheme, color_provider, is_in_web_app_scope);
}

bool StyleColor::IsColorKeyword(CSSValueID id) {
  // Named colors and color keywords:
  //
  // <named-color>
  //   'aqua', 'black', 'blue', ..., 'yellow' (CSS3: "basic color keywords")
  //   'aliceblue', ..., 'yellowgreen'        (CSS3: "extended color keywords")
  //   'transparent'
  //
  // 'currentcolor'
  //
  // <deprecated-system-color>
  //   'ActiveBorder', ..., 'WindowText'
  //
  // WebKit proprietary/internal:
  //   '-webkit-link'
  //   '-webkit-activelink'
  //   '-internal-active-list-box-selection'
  //   '-internal-active-list-box-selection-text'
  //   '-internal-inactive-list-box-selection'
  //   '-internal-inactive-list-box-selection-text'
  //   '-webkit-focus-ring-color'
  //   '-internal-quirk-inherit'
  //
  // css-text-decor
  // <https://github.com/w3c/csswg-drafts/issues/7522>
  //   '-internal-spelling-error-color'
  //   '-internal-grammar-error-color'
  //
  // ::search-text
  // <https://github.com/w3c/csswg-drafts/issues/10329>
  //   ‘-internal-search-color’
  //   ‘-internal-search-text-color’
  //   ‘-internal-current-search-color’
  //   ‘-internal-current-search-text-color’
  //
  return (id >= CSSValueID::kAqua &&
          id <= CSSValueID::kInternalCurrentSearchTextColor) ||
         (id >= CSSValueID::kAliceblue && id <= CSSValueID::kYellowgreen) ||
         id == CSSValueID::kMenu;
}

Color StyleColor::GetColor() const {
  // System colors will fail the IsNumeric check, as they store a keyword, but
  // they also have a stored color that may need to be accessed directly. For
  // example in FilterEffectBuilder::BuildFilterEffect for shadow colors.
  // Unresolved color mix functions do not yet have a stored color.

  DCHECK(!IsUnresolvedColorMixFunction());
  DCHECK(IsNumeric() || IsSystemColorIncludingDeprecated());
  return color_or_unresolved_color_mix_.color;
}

bool StyleColor::IsSystemColorIncludingDeprecated(CSSValueID id) {
  return (id >= CSSValueID::kActiveborder && id <= CSSValueID::kWindowtext) ||
         id == CSSValueID::kMenu;
}

bool StyleColor::IsSystemColor(CSSValueID id) {
  switch (id) {
    case CSSValueID::kAccentcolor:
    case CSSValueID::kAccentcolortext:
    case CSSValueID::kActivetext:
    case CSSValueID::kButtonborder:
    case CSSValueID::kButtonface:
    case CSSValueID::kButtontext:
    case CSSValueID::kCanvas:
    case CSSValueID::kCanvastext:
    case CSSValueID::kField:
    case CSSValueID::kFieldtext:
    case CSSValueID::kGraytext:
    case CSSValueID::kHighlight:
    case CSSValueID::kHighlighttext:
    case CSSValueID::kInternalGrammarErrorColor:
    case CSSValueID::kInternalSpellingErrorColor:
    case CSSValueID::kInternalSearchColor:
    case CSSValueID::kInternalSearchTextColor:
    case CSSValueID::kInternalCurrentSearchColor:
    case CSSValueID::kInternalCurrentSearchTextColor:
    case CSSValueID::kLinktext:
    case CSSValueID::kMark:
    case CSSValueID::kMarktext:
    case CSSValueID::kSelecteditem:
    case CSSValueID::kSelecteditemtext:
    case CSSValueID::kVisitedtext:
      return true;
    default:
      return false;
  }
}

CSSValueID StyleColor::EffectiveColorKeyword() const {
  return IsSystemColorIncludingDeprecated(color_keyword_) ? CSSValueID::kInvalid
                                                          : color_keyword_;
}

CORE_EXPORT std::ostream& operator<<(std::ostream& stream,
                                     const StyleColor& color) {
  if (color.IsCurrentColor()) {
    return stream << "currentcolor";
  } else if (color.IsUnresolvedColorMixFunction()) {
    return stream << "<unresolved color-mix>";
  } else if (color.HasColorKeyword() && !color.IsNumeric()) {
    return stream << getValueName(color.GetColorKeyword());
  } else {
    return stream << color.GetColor();
  }
}

}  // namespace blink
