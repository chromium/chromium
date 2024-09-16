// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_color.h"

#include <memory>

#include "third_party/blink/renderer/core/css/color_function.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_channel_keywords.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_relative_color_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

namespace {

using UnderlyingColorType = StyleColor::UnderlyingColorType;

UnderlyingColorType ResolveColorOperandType(const StyleColor& c) {
  if (c.IsUnresolvedColorFunction()) {
    return UnderlyingColorType::kColorFunction;
  }
  if (c.IsCurrentColor()) {
    return UnderlyingColorType::kCurrentColor;
  }
  return UnderlyingColorType::kColor;
}

Color ResolveColorOperand(
    const StyleColor::ColorOrUnresolvedColorFunction& color,
    UnderlyingColorType type,
    const Color& current_color) {
  switch (type) {
    case UnderlyingColorType::kColorFunction:
      return color.unresolved_color_function->Resolve(current_color);
    case UnderlyingColorType::kCurrentColor:
      return current_color;
    case UnderlyingColorType::kColor:
      return color.color;
  }
}

CSSValue* ConvertColorOperandToCSSValue(
    const StyleColor::ColorOrUnresolvedColorFunction& color_or_function,
    UnderlyingColorType type) {
  switch (type) {
    case UnderlyingColorType::kColor:
      return cssvalue::CSSColor::Create(color_or_function.color);
    case UnderlyingColorType::kColorFunction:
      CHECK(color_or_function.unresolved_color_function);
      return color_or_function.unresolved_color_function->ToCSSValue();
    case UnderlyingColorType::kCurrentColor:
      return CSSIdentifierValue::Create(CSSValueID::kCurrentcolor);
  }
}

}  // namespace

CORE_EXPORT bool StyleColor::UnresolvedColorFunction::operator==(
    const UnresolvedColorFunction& other) const {
  if (type_ != other.GetType()) {
    return false;
  }

  switch (type_) {
    case StyleColor::UnresolvedColorFunction::Type::kColorMix:
      return *To<UnresolvedColorMix>(this) == To<UnresolvedColorMix>(other);
    case StyleColor::UnresolvedColorFunction::Type::kRelativeColor:
      return *To<UnresolvedRelativeColor>(this) ==
             To<UnresolvedRelativeColor>(other);
  }

  NOTREACHED();
}

StyleColor::UnresolvedColorMix::UnresolvedColorMix(
    Color::ColorSpace color_interpolation_space,
    Color::HueInterpolationMethod hue_interpolation_method,
    const StyleColor& c1,
    const StyleColor& c2,
    double percentage,
    double alpha_multiplier)
    : UnresolvedColorFunction(UnresolvedColorFunction::Type::kColorMix),
      color_interpolation_space_(color_interpolation_space),
      hue_interpolation_method_(hue_interpolation_method),
      color1_(c1.color_or_unresolved_color_function_),
      color2_(c2.color_or_unresolved_color_function_),
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

CSSValue* StyleColor::UnresolvedColorMix::ToCSSValue() const {
  const CSSPrimitiveValue* percent1 = CSSNumericLiteralValue::Create(
      100 * (1.0 - percentage_) * alpha_multiplier_,
      CSSPrimitiveValue::UnitType::kPercentage);
  const CSSPrimitiveValue* percent2 =
      CSSNumericLiteralValue::Create(100 * percentage_ * alpha_multiplier_,
                                     CSSPrimitiveValue::UnitType::kPercentage);

  return MakeGarbageCollected<cssvalue::CSSColorMixValue>(
      ConvertColorOperandToCSSValue(color1_, color1_type_),
      ConvertColorOperandToCSSValue(color2_, color2_type_), percent1, percent2,
      color_interpolation_space_, hue_interpolation_method_);
}

StyleColor::UnresolvedRelativeColor::UnresolvedRelativeColor(
    const StyleColor& origin_color,
    Color::ColorSpace color_interpolation_space,
    const CSSValue& channel0,
    const CSSValue& channel1,
    const CSSValue& channel2,
    const CSSValue* alpha)
    : UnresolvedColorFunction(UnresolvedColorFunction::Type::kRelativeColor),
      origin_color_(origin_color.color_or_unresolved_color_function_),
      origin_color_type_(ResolveColorOperandType(origin_color)),
      color_interpolation_space_(color_interpolation_space) {
  auto to_channel =
      [](const CSSValue& value) -> scoped_refptr<const CalculationValue> {
    if (const CSSNumericLiteralValue* numeric =
            DynamicTo<CSSNumericLiteralValue>(value)) {
      if (numeric->IsPercentage()) {
        return CalculationValue::Create(
            PixelsAndPercent(0., numeric->DoubleValue(), false, true),
            Length::ValueRange::kAll);
      } else {
        // It's not actually a "pixels" value, but treating it as one simplifies
        // storage and resolution.
        return CalculationValue::Create(
            PixelsAndPercent(numeric->DoubleValue()), Length::ValueRange::kAll);
      }
    } else if (const CSSIdentifierValue* identifier =
                   DynamicTo<CSSIdentifierValue>(value)) {
      if (identifier->GetValueID() == CSSValueID::kNone) {
        return nullptr;
      }
      scoped_refptr<CalculationExpressionNode> expression =
          base::MakeRefCounted<CalculationExpressionColorChannelKeywordNode>(
              CSSValueIDToColorChannelKeyword(identifier->GetValueID()));
      return CalculationValue::CreateSimplified(std::move(expression),
                                                Length::ValueRange::kAll);
    } else if (const CSSMathFunctionValue* function =
                   DynamicTo<CSSMathFunctionValue>(value)) {
      return function->ToCalcValue(CSSToLengthConversionData());
    } else {
      NOTREACHED();
    }
  };

  channel0_ = to_channel(channel0);
  channel1_ = to_channel(channel1);
  channel2_ = to_channel(channel2);
  if (alpha != nullptr) {
    alpha_was_specified_ = true;
    alpha_ = to_channel(*alpha);
  } else {
    // https://drafts.csswg.org/css-color-5/#rcs-intro
    // If the alpha value of the relative color is omitted, it defaults to that
    // of the origin color (rather than defaulting to 100%, as it does in the
    // absolute syntax).
    alpha_was_specified_ = false;
    scoped_refptr<CalculationExpressionNode> expression =
        base::MakeRefCounted<CalculationExpressionColorChannelKeywordNode>(
            ColorChannelKeyword::kAlpha);
    alpha_ = CalculationValue::CreateSimplified(std::move(expression),
                                                Length::ValueRange::kAll);
  }
}

void StyleColor::UnresolvedRelativeColor::Trace(Visitor* visitor) const {
  UnresolvedColorFunction::Trace(visitor);
  visitor->Trace(origin_color_);
}

CSSValue* StyleColor::UnresolvedRelativeColor::ToCSSValue() const {
  auto to_css_value = [](const scoped_refptr<const CalculationValue>& channel)
      -> const CSSValue* {
    if (channel == nullptr) {
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    }
    if (!channel->IsExpression()) {
      if (channel->HasExplicitPercent()) {
        return CSSNumericLiteralValue::Create(
            channel->Percent(), CSSPrimitiveValue::UnitType::kPercentage);
      } else {
        return CSSNumericLiteralValue::Create(
            channel->Pixels(), CSSPrimitiveValue::UnitType::kNumber);
      }
    }
    scoped_refptr<const CalculationExpressionNode> expression =
        channel->GetOrCreateExpression();
    if (expression->IsColorChannelKeyword()) {
      return CSSIdentifierValue::Create(ColorChannelKeywordToCSSValueID(
          To<CalculationExpressionColorChannelKeywordNode>(expression.get())
              ->Value()));
    } else {
      return CSSMathFunctionValue::Create(
          CSSMathExpressionNode::Create(*channel));
    }
  };

  const CSSValue* channel0 = to_css_value(channel0_);
  const CSSValue* channel1 = to_css_value(channel1_);
  const CSSValue* channel2 = to_css_value(channel2_);
  const CSSValue* alpha = alpha_was_specified_ ? to_css_value(alpha_) : nullptr;

  return MakeGarbageCollected<cssvalue::CSSRelativeColorValue>(
      *ConvertColorOperandToCSSValue(origin_color_, origin_color_type_),
      color_interpolation_space_, *channel0, *channel1, *channel2, alpha);
}

Color StyleColor::UnresolvedRelativeColor::Resolve(
    const Color& current_color) const {
  Color resolved_origin =
      ResolveColorOperand(origin_color_, origin_color_type_, current_color);
  resolved_origin.ConvertToColorSpace(color_interpolation_space_);

  const ColorFunction::Metadata& function_metadata =
      ColorFunction::MetadataForColorSpace(color_interpolation_space_);

  std::vector<std::pair<ColorChannelKeyword, float>> keyword_values = {
      {{CSSValueIDToColorChannelKeyword(function_metadata.channel_name[0]),
        resolved_origin.Param0()},
       {CSSValueIDToColorChannelKeyword(function_metadata.channel_name[1]),
        resolved_origin.Param1()},
       {CSSValueIDToColorChannelKeyword(function_metadata.channel_name[2]),
        resolved_origin.Param2()},
       {ColorChannelKeyword::kAlpha, resolved_origin.Alpha()}}};

  // We need to make value adjustments for certain color spaces.
  //
  // https://www.w3.org/TR/css-color-4/#the-hsl-notation
  // https://www.w3.org/TR/css-color-4/#the-hwb-notation
  // hsl and hwb are specified with percent reference ranges of 0..100 in
  // channels 1 and 2, but blink::Color represents these values over 0..1.
  // We scale up the origin values so that they pass through computation
  // correctly, then later, scale them down in the final result.
  //
  // https://www.w3.org/TR/css-color-4/#hue-syntax
  // Channels representing <hue> are normalized to the range [0,360).
  const bool is_hxx_color_space =
      (color_interpolation_space_ == Color::ColorSpace::kHSL) ||
      (color_interpolation_space_ == Color::ColorSpace::kHWB);
  const bool is_lch_color_space =
      (color_interpolation_space_ == Color::ColorSpace::kLch) ||
      (color_interpolation_space_ == Color::ColorSpace::kOklch);

  if (is_hxx_color_space) {
    keyword_values[1].second *= 100.;
    keyword_values[2].second *= 100.;
  }

  EvaluationInput evaluation_input;
  evaluation_input.color_channel_keyword_values =
      base::flat_map(std::move(keyword_values));

  auto to_channel_value =
      [&evaluation_input](const CalculationValue* calculation_value,
                          double channel_percentage) -> std::optional<float> {
    // The color function metadata table uses NaN to indicate that percentages
    // are not applicable to a given channel. NaN is not suitable as a clamp
    // limit for evaluating a CalculationValue, so translate it into float max.
    const float max_value = (std::isnan(channel_percentage))
                                ? std::numeric_limits<float>::max()
                                : channel_percentage;
    if (calculation_value != nullptr) {
      return calculation_value->Evaluate(max_value, evaluation_input);
    }
    return std::nullopt;
  };

  std::array<std::optional<float>, 3> params = {
      to_channel_value(channel0_.get(),
                       function_metadata.channel_percentage[0]),
      to_channel_value(channel1_.get(),
                       function_metadata.channel_percentage[1]),
      to_channel_value(channel2_.get(),
                       function_metadata.channel_percentage[2])};
  std::optional<float> param_alpha = to_channel_value(alpha_.get(), 1.f);

  auto wrap_hue_channel = [](std::optional<float>& param) {
    if (param.has_value()) {
      // Perform the wrap at double precision to avoid floating-point rounding
      // drift which is observable at single precision for some values.
      param.value() =
          fmod(fmod(static_cast<double>(param.value()), 360.0) + 360.0, 360.0);
    }
  };
  auto scale_down_channel = [](std::optional<float>& param) {
    if (param.has_value()) {
      param.value() /= 100.f;
    }
  };
  if (is_hxx_color_space) {
    wrap_hue_channel(params[0]);
    scale_down_channel(params[1]);
    scale_down_channel(params[2]);
  } else if (is_lch_color_space) {
    wrap_hue_channel(params[2]);
  }

  Color result = Color::FromColorSpace(color_interpolation_space_, params[0],
                                       params[1], params[2], param_alpha);
  if (Color::IsLegacyColorSpace(result.GetColorSpace())) {
    result.ConvertToColorSpace(Color::ColorSpace::kSRGB);
  }
  return result;
}

bool StyleColor::UnresolvedRelativeColor::operator==(
    const UnresolvedRelativeColor& other) const {
  if (origin_color_type_ != other.origin_color_type_ ||
      color_interpolation_space_ != other.color_interpolation_space_ ||
      alpha_was_specified_ != other.alpha_was_specified_ ||
      !base::ValuesEquivalent(channel0_, other.channel0_) ||
      !base::ValuesEquivalent(channel1_, other.channel1_) ||
      !base::ValuesEquivalent(channel2_, other.channel2_) ||
      !base::ValuesEquivalent(alpha_, other.alpha_)) {
    return false;
  }

  return ColorOrUnresolvedColorFunction::Equals(
      origin_color_, other.origin_color_, origin_color_type_);
}

void StyleColor::ColorOrUnresolvedColorFunction::Trace(Visitor* visitor) const {
  visitor->Trace(unresolved_color_function);
}

Color StyleColor::Resolve(const Color& current_color,
                          mojom::blink::ColorScheme color_scheme,
                          bool* is_current_color) const {
  if (IsUnresolvedColorFunction()) {
    return color_or_unresolved_color_function_.unresolved_color_function
        ->Resolve(current_color);
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
  // Unresolved color functions do not yet have a stored color.
  DCHECK(!IsUnresolvedColorFunction());
  DCHECK(IsNumeric() || IsSystemColorIncludingDeprecated());
  return color_or_unresolved_color_function_.color;
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
  } else if (color.IsUnresolvedColorFunction()) {
    return stream << color.GetUnresolvedColorFunction();
  } else if (color.HasColorKeyword() && !color.IsNumeric()) {
    return stream << getValueName(color.GetColorKeyword());
  } else {
    return stream << color.GetColor();
  }
}

CORE_EXPORT std::ostream& operator<<(
    std::ostream& stream,
    const StyleColor::UnresolvedColorFunction& unresolved_color_function) {
  return stream << unresolved_color_function.ToCSSValue()->CssText();
}

}  // namespace blink
