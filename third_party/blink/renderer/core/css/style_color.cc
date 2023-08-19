// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_color.h"
#include <memory>

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace blink {

StyleColor::UnresolvedColorMix::UnresolvedColorMix(
    const cssvalue::CSSColorMixValue* in,
    const StyleColor& c1,
    const StyleColor& c2)
    : color_interpolation_space_(in->ColorInterpolationSpace()),
      hue_interpolation_method_(in->HueInterpolationMethod()),
      color1_(c1),
      color2_(c2) {
  if (c1.IsUnresolvedColorMixFunction()) {
    color1_type_ = UnderlyingColorType::kColorMix;
  } else if (c1.IsCurrentColor()) {
    color1_type_ = UnderlyingColorType::kCurrentColor;
  } else {
    color1_type_ = UnderlyingColorType::kColor;
  }

  if (c2.IsUnresolvedColorMixFunction()) {
    color2_type_ = UnderlyingColorType::kColorMix;
  } else if (c2.IsCurrentColor()) {
    color2_type_ = UnderlyingColorType::kCurrentColor;
  } else {
    color2_type_ = UnderlyingColorType::kColor;
  }

  // TODO(crbug.com/1333988): If both percentages are zero, the color should
  // be rejected at parse time.
  cssvalue::CSSColorMixValue::NormalizePercentages(
      in->Percentage1(), in->Percentage2(), percentage_, alpha_multiplier_);
}

StyleColor::UnresolvedColorMix::UnresolvedColorMix(
    const UnresolvedColorMix& other)
    : color_interpolation_space_(other.color_interpolation_space_),
      hue_interpolation_method_(other.hue_interpolation_method_),
      percentage_(other.percentage_),
      alpha_multiplier_(other.alpha_multiplier_),
      color1_type_(other.color1_type_),
      color2_type_(other.color2_type_) {
  if (color1_type_ == UnderlyingColorType::kColorMix) {
    new (&color1_.unresolved_color_mix) std::unique_ptr<UnresolvedColorMix>(
        new UnresolvedColorMix(*other.color1_.unresolved_color_mix));
  } else if (color1_type_ == UnderlyingColorType::kColor) {
    color1_.color = other.color1_.color;
  }

  if (color2_type_ == UnderlyingColorType::kColorMix) {
    new (&color2_.unresolved_color_mix) std::unique_ptr<UnresolvedColorMix>(
        new UnresolvedColorMix(*other.color2_.unresolved_color_mix));
  } else if (color2_type_ == UnderlyingColorType::kColor) {
    color2_.color = other.color2_.color;
  }
}

StyleColor::UnresolvedColorMix& StyleColor::UnresolvedColorMix::operator=(
    const StyleColor::UnresolvedColorMix& other) {
  if (this == &other) {
    return *this;
  }
  color_interpolation_space_ = other.color_interpolation_space_;
  hue_interpolation_method_ = other.hue_interpolation_method_;
  percentage_ = other.percentage_;
  alpha_multiplier_ = other.alpha_multiplier_;

  if (other.color1_type_ == UnderlyingColorType::kColorMix) {
    if (color1_type_ == UnderlyingColorType::kColorMix) {
      // Avoid leaking an UnresolvedColorMix that is already stored on "this"
      color1_.unresolved_color_mix.reset();
      color1_.unresolved_color_mix = std::make_unique<UnresolvedColorMix>(
          *other.color1_.unresolved_color_mix);
    } else {
      new (&color1_.unresolved_color_mix) std::unique_ptr<UnresolvedColorMix>(
          new UnresolvedColorMix(*other.color1_.unresolved_color_mix));
    }
  } else if (other.color1_type_ == UnderlyingColorType::kColor) {
    color1_.color = other.color1_.color;
  }

  if (other.color2_type_ == UnderlyingColorType::kColorMix) {
    if (color2_type_ == UnderlyingColorType::kColorMix) {
      // Avoid leaking an UnresolvedColorMix that is already stored on "this"
      color2_.unresolved_color_mix.reset();
      color2_.unresolved_color_mix = std::make_unique<UnresolvedColorMix>(
          *other.color2_.unresolved_color_mix);
    } else {
      new (&color2_.unresolved_color_mix) std::unique_ptr<UnresolvedColorMix>(
          new UnresolvedColorMix(*other.color2_.unresolved_color_mix));
    }
  } else if (other.color2_type_ == UnderlyingColorType::kColor) {
    color2_.color = other.color2_.color;
  }

  color1_type_ = other.color1_type_;
  color2_type_ = other.color2_type_;
  return *this;
}

Color StyleColor::UnresolvedColorMix::Resolve(
    const Color& current_color) const {
  Color c1 = current_color;
  if (color1_type_ ==
      StyleColor::UnresolvedColorMix::UnderlyingColorType::kColor) {
    c1 = color1_.color;
  } else if (color1_type_ ==
             StyleColor::UnresolvedColorMix::UnderlyingColorType::kColorMix) {
    c1 = color1_.unresolved_color_mix->Resolve(current_color);
  }

  Color c2 = current_color;
  if (color2_type_ ==
      StyleColor::UnresolvedColorMix::UnderlyingColorType::kColor) {
    c2 = color2_.color;
  } else if (color2_type_ ==
             StyleColor::UnresolvedColorMix::UnderlyingColorType::kColorMix) {
    c2 = color2_.unresolved_color_mix->Resolve(current_color);
  }

  return Color::FromColorMix(color_interpolation_space_,
                             hue_interpolation_method_, c1, c2, percentage_,
                             alpha_multiplier_);
}

StyleColor::ColorOrUnresolvedColorMix::ColorOrUnresolvedColorMix(
    UnresolvedColorMix color_mix) {
  new (&unresolved_color_mix)
      std::unique_ptr<UnresolvedColorMix>(new UnresolvedColorMix(color_mix));
}

StyleColor::ColorOrUnresolvedColorMix::ColorOrUnresolvedColorMix(
    const StyleColor style_color) {
  if (style_color.IsUnresolvedColorMixFunction()) {
    new (&unresolved_color_mix) std::unique_ptr<UnresolvedColorMix>(
        new UnresolvedColorMix(style_color.GetUnresolvedColorMix()));
  } else {
    color = style_color.color_or_unresolved_color_mix_.color;
  }
}

StyleColor::StyleColor(const StyleColor& other)
    : color_keyword_(other.color_keyword_) {
  if (IsUnresolvedColorMixFunction()) {
    new (&color_or_unresolved_color_mix_.unresolved_color_mix)
        std::unique_ptr<UnresolvedColorMix>(new UnresolvedColorMix(
            *other.color_or_unresolved_color_mix_.unresolved_color_mix));
  } else {
    color_or_unresolved_color_mix_.color =
        other.color_or_unresolved_color_mix_.color;
  }
}

StyleColor& StyleColor::operator=(const StyleColor& other) {
  if (this == &other) {
    return *this;
  }
  if (other.IsUnresolvedColorMixFunction()) {
    if (IsUnresolvedColorMixFunction()) {
      color_or_unresolved_color_mix_.unresolved_color_mix.reset();
      color_or_unresolved_color_mix_.unresolved_color_mix =
          std::make_unique<UnresolvedColorMix>(
              *other.color_or_unresolved_color_mix_.unresolved_color_mix);
    } else {
      new (&color_or_unresolved_color_mix_.unresolved_color_mix)
          std::unique_ptr<UnresolvedColorMix>(new UnresolvedColorMix(
              *other.color_or_unresolved_color_mix_.unresolved_color_mix));
    }
  } else {
    color_or_unresolved_color_mix_.color =
        other.color_or_unresolved_color_mix_.color;
  }
  color_keyword_ = other.color_keyword_;
  return *this;
}

StyleColor::StyleColor(StyleColor&& other)
    : color_keyword_(other.color_keyword_) {
  if (other.IsUnresolvedColorMixFunction()) {
    new (&color_or_unresolved_color_mix_.unresolved_color_mix)
        std::unique_ptr<UnresolvedColorMix>(std::move(
            other.color_or_unresolved_color_mix_.unresolved_color_mix));
  } else {
    color_or_unresolved_color_mix_.color =
        other.color_or_unresolved_color_mix_.color;
  }
}

StyleColor& StyleColor::operator=(StyleColor&& other) {
  if (this == &other) {
    return *this;
  }
  if (other.IsUnresolvedColorMixFunction()) {
    if (IsUnresolvedColorMixFunction()) {
      color_or_unresolved_color_mix_.unresolved_color_mix.reset();
      color_or_unresolved_color_mix_.unresolved_color_mix =
          std::make_unique<UnresolvedColorMix>(std::move(
              *other.color_or_unresolved_color_mix_.unresolved_color_mix));
    } else {
      new (&color_or_unresolved_color_mix_.unresolved_color_mix)
          std::unique_ptr<UnresolvedColorMix>((std::move(
              other.color_or_unresolved_color_mix_.unresolved_color_mix)));
    }
  } else {
    color_or_unresolved_color_mix_.color =
        other.color_or_unresolved_color_mix_.color;
  }
  color_keyword_ = other.color_keyword_;
  return *this;
}

StyleColor::~StyleColor() {
  if (IsUnresolvedColorMixFunction()) {
    color_or_unresolved_color_mix_.unresolved_color_mix.reset();
  }
}

Color StyleColor::Resolve(const Color& current_color,
                          mojom::blink::ColorScheme color_scheme,
                          bool* is_current_color,
                          bool is_forced_color) const {
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
  if (EffectiveColorKeyword() != CSSValueID::kInvalid ||
      (is_forced_color && IsSystemColorIncludingDeprecated())) {
    return ColorFromKeyword(color_keyword_, color_scheme);
  }
  return GetColor();
}

Color StyleColor::ResolveWithAlpha(Color current_color,
                                   mojom::blink::ColorScheme color_scheme,
                                   int alpha,
                                   bool* is_current_color,
                                   bool is_forced_color) const {
  Color color =
      Resolve(current_color, color_scheme, is_current_color, is_forced_color);
  // TODO(crbug.com/1333988) This looks unfriendly to CSS Color 4.
  return Color(color.Red(), color.Green(), color.Blue(), alpha);
}

Color StyleColor::ColorFromKeyword(CSSValueID keyword,
                                   mojom::blink::ColorScheme color_scheme) {
  if (const char* value_name = getValueName(keyword)) {
    if (const NamedColor* named_color = FindColor(
            value_name, static_cast<wtf_size_t>(strlen(value_name)))) {
      return Color::FromRGBA32(named_color->argb_value);
    }
  }
  return LayoutTheme::GetTheme().SystemColor(keyword, color_scheme);
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
  return (id >= CSSValueID::kAqua &&
          id <= CSSValueID::kInternalGrammarErrorColor) ||
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
