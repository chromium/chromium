// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_color_function_parser.h"

#include <cmath>

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_channel_keywords.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_relative_color_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_unresolved_color_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"

namespace blink {

namespace {

// https://www.w3.org/TR/css-color-4/#typedef-color-function
bool IsValidColorFunction(CSSValueID id) {
  switch (id) {
    case CSSValueID::kRgb:
    case CSSValueID::kRgba:
    case CSSValueID::kHsl:
    case CSSValueID::kHsla:
    case CSSValueID::kHwb:
    case CSSValueID::kLab:
    case CSSValueID::kLch:
    case CSSValueID::kOklab:
    case CSSValueID::kOklch:
    case CSSValueID::kColor:
      return true;
    default:
      return false;
  }
}

Color::ColorSpace ColorSpaceFromFunctionName(CSSValueID id) {
  switch (id) {
    case CSSValueID::kRgb:
    case CSSValueID::kRgba:
      return Color::ColorSpace::kSRGBLegacy;
    case CSSValueID::kHsl:
    case CSSValueID::kHsla:
      return Color::ColorSpace::kHSL;
    case CSSValueID::kHwb:
      return Color::ColorSpace::kHWB;
    case CSSValueID::kLab:
      return Color::ColorSpace::kLab;
    case CSSValueID::kOklab:
      return Color::ColorSpace::kOklab;
    case CSSValueID::kLch:
      return Color::ColorSpace::kLch;
    case CSSValueID::kOklch:
      return Color::ColorSpace::kOklch;
    default:
      return Color::ColorSpace::kNone;
  }
}

// https://www.w3.org/TR/css-color-4/#color-function
Color::ColorSpace ColorSpaceFromColorSpaceArgument(CSSValueID id) {
  switch (id) {
    case CSSValueID::kSRGB:
      return Color::ColorSpace::kSRGB;
    case CSSValueID::kRec2020:
      return Color::ColorSpace::kRec2020;
    case CSSValueID::kRec2100Linear:
      if (RuntimeEnabledFeatures::ColorSpaceRec2100LinearEnabled()) {
        return Color::ColorSpace::kRec2100Linear;
      } else {
        return Color::ColorSpace::kNone;
      }
    case CSSValueID::kSRGBLinear:
      return Color::ColorSpace::kSRGBLinear;
    case CSSValueID::kDisplayP3:
      return Color::ColorSpace::kDisplayP3;
    case CSSValueID::kDisplayP3Linear:
      if (RuntimeEnabledFeatures::ColorSpaceDisplayP3LinearEnabled()) {
        return Color::ColorSpace::kDisplayP3Linear;
      } else {
        return Color::ColorSpace::kNone;
      }
    case CSSValueID::kA98Rgb:
      return Color::ColorSpace::kA98RGB;
    case CSSValueID::kProphotoRgb:
      return Color::ColorSpace::kProPhotoRGB;
    case CSSValueID::kXyzD50:
      return Color::ColorSpace::kXYZD50;
    case CSSValueID::kXyz:
    case CSSValueID::kXyzD65:
      return Color::ColorSpace::kXYZD65;
    default:
      return Color::ColorSpace::kNone;
  }
}

bool ColorChannelIsHue(Color::ColorSpace color_space, int channel) {
  if (color_space == Color::ColorSpace::kHSL ||
      color_space == Color::ColorSpace::kHWB) {
    if (channel == 0) {
      return true;
    }
  }
  if (color_space == Color::ColorSpace::kLch ||
      color_space == Color::ColorSpace::kOklch) {
    if (channel == 2) {
      return true;
    }
  }
  return false;
}

CSSValue* ConsumeRelativeColorChannel(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSColorChannelMap& color_channel_map,
    CalculationResultCategorySet expected_categories,
    const double percentage_base = 0) {
  const CSSParserToken token = stream.Peek();
  // Relative color channels can be calc() functions with color channel
  // replacements. e.g. In "color(from magenta srgb calc(r / 2) 0 0)", the
  // "calc" should substitute "1" for "r" (magenta has a full red channel).
  if (token.GetType() == kFunctionToken) {
    using enum CSSMathExpressionNode::Flag;
    using Flags = CSSMathExpressionNode::Flags;

    // Don't consume the range if the parsing fails.
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    CSSMathFunctionValue* calc_value = CSSMathFunctionValue::Create(
        CSSMathExpressionNode::ParseMathFunction(
            token.FunctionId(), stream, context, Flags({AllowPercent}),
            kCSSAnchorQueryTypesNone, color_channel_map),
        CSSPrimitiveValue::ValueRange::kAll);
    if (calc_value) {
      const CalculationResultCategory category = calc_value->Category();
      if (!expected_categories.Has(category)) {
        return nullptr;
      }
      // Consume the range, since it has succeeded.
      guard.Release();
      stream.ConsumeWhitespace();
      return calc_value;
    }
  }

  // This is for just single variable swaps without calc(). e.g. The "l" in
  // "lab(from cyan l 0.5 0.5)".
  if (color_channel_map.Contains(token.Id())) {
    return css_parsing_utils::ConsumeIdent(stream);
  }

  return nullptr;
}

// Returns true if, when converted to Rec2020 space, all components of `color`
// are in the interval [-1/255, 256/255].
bool IsInGamutRec2020(Color color) {
  const float kEpsilon = 1 / 255.f;
  color.ConvertToColorSpace(Color::ColorSpace::kRec2020);
  return -kEpsilon <= color.Param0() && color.Param0() <= 1.f + kEpsilon &&
         -kEpsilon <= color.Param1() && color.Param1() <= 1.f + kEpsilon &&
         -kEpsilon <= color.Param2() && color.Param2() <= 1.f + kEpsilon;
}

}  // namespace

bool ColorFunctionParser::ConsumeColorSpaceAndOriginColor(
    CSSParserTokenStream& stream,
    CSSValueID function_id,
    const CSSParserContext& context,
    const css_parsing_utils::ColorParserContext& color_parser_context) {
  // [from <color>]?
  if (css_parsing_utils::ConsumeIdent<CSSValueID::kFrom>(stream)) {
    unresolved_origin_color_ =
        css_parsing_utils::ConsumeColor(stream, context, color_parser_context);
    if (!unresolved_origin_color_) {
      return false;
    }
  }

  // Get the color space. This will either be the name of the function, or it
  // will be the first argument of the "color" function.
  if (function_id == CSSValueID::kColor) {
    // <predefined-rgb> | <xyz-space>
    if (stream.Peek().GetType() != kIdentToken) {
      return false;
    }
    color_space_ = ColorSpaceFromColorSpaceArgument(
        stream.ConsumeIncludingWhitespace().Id());
    if (color_space_ == Color::ColorSpace::kNone) {
      return false;
    }
  } else {
    color_space_ = ColorSpaceFromFunctionName(function_id);
  }

  function_metadata_ = &ColorFunction::MetadataForColorSpace(color_space_);

  if (unresolved_origin_color_) {
    // Fill out the map with the valid channel names. We need that information
    // to parse the remainder of the color function.
    color_channel_map_ = {
        {function_metadata_->channel_name[0], std::nullopt},
        {function_metadata_->channel_name[1], std::nullopt},
        {function_metadata_->channel_name[2], std::nullopt},
        {CSSValueID::kAlpha, std::nullopt},
    };
  }
  return true;
}

namespace {

bool IsAllowedValueInParserContext(
    const CSSValue* value,
    const css_parsing_utils::ColorParserContext& color_parser_context) {
  if (auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    return color_parser_context.InElementContext() ||
           !primitive_value->IsElementDependent();
  }
  return true;
}

}  // namespace

bool ColorFunctionParser::ConsumeChannel(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    int i,
    const css_parsing_utils::ColorParserContext& color_parser_context) {
  if (css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(stream)) {
    unresolved_channels_[i] = CSSIdentifierValue::Create(CSSValueID::kNone);
    channel_types_[i] = ChannelType::kNone;
    has_none_ = true;
    return true;
  }

  if (ColorChannelIsHue(color_space_, i)) {
    if ((unresolved_channels_[i] =
             css_parsing_utils::ConsumeAngle(stream, context, std::nullopt))) {
      channel_types_[i] = ChannelType::kNumber;
    } else if ((unresolved_channels_[i] = css_parsing_utils::ConsumeNumber(
                    stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
      channel_types_[i] = ChannelType::kNumber;
    } else if (IsRelativeColor()) {
      if ((unresolved_channels_[i] =
               ConsumeRelativeColorChannel(stream, context, color_channel_map_,
                                           {kCalcNumber, kCalcAngle}))) {
        channel_types_[i] = ChannelType::kRelative;
      }
    }

    if (!unresolved_channels_[i]) {
      return false;
    }

    return IsAllowedValueInParserContext(unresolved_channels_[i],
                                         color_parser_context);
  }

  if ((unresolved_channels_[i] = css_parsing_utils::ConsumeNumber(
           stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
    channel_types_[i] = ChannelType::kNumber;
    return IsAllowedValueInParserContext(unresolved_channels_[i],
                                         color_parser_context);
  }

  if ((unresolved_channels_[i] = css_parsing_utils::ConsumePercent(
           stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
    channel_types_[i] = ChannelType::kPercentage;
    return IsAllowedValueInParserContext(unresolved_channels_[i],
                                         color_parser_context);
  }

  if (IsRelativeColor()) {
    channel_types_[i] = ChannelType::kRelative;
    if ((unresolved_channels_[i] = ConsumeRelativeColorChannel(
             stream, context, color_channel_map_, {kCalcNumber, kCalcPercent},
             function_metadata_->channel_percentage[i]))) {
      return IsAllowedValueInParserContext(unresolved_channels_[i],
                                           color_parser_context);
    }
  }

  // Missing components should not parse.
  return false;
}

bool ColorFunctionParser::ConsumeAlpha(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const css_parsing_utils::ColorParserContext& color_parser_context) {
  if ((unresolved_alpha_ = css_parsing_utils::ConsumeNumber(
           stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
    alpha_channel_type_ = ChannelType::kNumber;
    return IsAllowedValueInParserContext(unresolved_alpha_,
                                         color_parser_context);
  }

  if ((unresolved_alpha_ = css_parsing_utils::ConsumePercent(
           stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
    alpha_channel_type_ = ChannelType::kPercentage;
    return IsAllowedValueInParserContext(unresolved_alpha_,
                                         color_parser_context);
  }

  if (css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(stream)) {
    has_none_ = true;
    unresolved_alpha_ = CSSIdentifierValue::Create(CSSValueID::kNone);
    alpha_channel_type_ = ChannelType::kNone;
    return true;
  }

  if (IsRelativeColor() && (unresolved_alpha_ = ConsumeRelativeColorChannel(
                                stream, context, color_channel_map_,
                                {kCalcNumber, kCalcPercent}, 1.0))) {
    alpha_channel_type_ = ChannelType::kRelative;
    return IsAllowedValueInParserContext(unresolved_alpha_,
                                         color_parser_context);
  }

  return false;
}

void ColorFunctionParser::MakePerColorSpaceAdjustments(
    bool is_relative_color,
    bool is_legacy_syntax,
    Color::ColorSpace color_space,
    std::array<std::optional<double>, 3>& channels,
    std::optional<double>& alpha) {
  for (int i = 0; i < 3; i++) {
    if (channels[i].has_value() && ColorChannelIsHue(color_space, i)) {
      // Non-finite values should be clamped to the range [0, 360].
      // Since 0 = 360 in this case, they can all simply become zero.
      if (!isfinite(channels[i].value())) {
        channels[i] = 0.0;
      }

      // Wrap hue to be in the range [0, 360].
      channels[i].value() =
          fmod(fmod(channels[i].value(), 360.0) + 360.0, 360.0);
    }
  }

  if (color_space == Color::ColorSpace::kSRGBLegacy) {
    for (int i = 0; i < 3; i++) {
      if (!channels[i].has_value()) {
        continue;
      }
      if (!isfinite(channels[i].value())) {
        channels[i].value() = channels[i].value() > 0 ? 255.0 : 0;
      } else if (!is_relative_color) {
        // Clamp to [0, 1] range, but allow out-of-gamut relative colors.
        channels[i].value() = ClampTo<double>(channels[i].value(), 0.0, 255.0);
      }
    }
    // TODO(crbug.com/1399566): There are many code paths that still compress
    // alpha to be an 8-bit integer. If it is not explicitly compressed here,
    // tests will fail due to some paths doing this compression and others not.
    // See compositing/background-color/background-color-alpha.html for example.
    // Ideally we would allow alpha to be any float value, but we have to clean
    // up all spots where this compression happens before this is possible.
    if (!is_relative_color && alpha.has_value()) {
      alpha = round(alpha.value() * 255.0) / 255.0;
    }
  }

  if (color_space == Color::ColorSpace::kHSL ||
      color_space == Color::ColorSpace::kHWB) {
    for (int i : {1, 2}) {
      // Raw numbers are interpreted as percentages in these color spaces.
      if (channels[i].has_value()) {
        double val = channels[i].value() / 100.0;

        if (is_legacy_syntax) {
          val = ClampTo<double>(val, 0.0, 1.0);
        } else if (!is_relative_color) {
          // In absolute colors, S/L/W/B are non-negative.
          val = std::max(0.0, val);
        }
        // Relative colors preserve out-of-gamut values for use in calculations.

        channels[i] = val;
      }
    }
  }
}

namespace {

const CSSNumericLiteralValue& GetNumericLiteralValue(const CSSValue& value) {
  // We can reach here with calc(NumericLiteral) as per ChannelIsResolvable.
  if (auto* math_value = DynamicTo<CSSMathFunctionValue>(value)) {
    const CSSMathExpressionNode* expression = math_value->ExpressionNode();
    DCHECK(expression->IsNumericLiteral());
    return *MakeGarbageCollected<CSSNumericLiteralValue>(
        expression->DoubleValue(), expression->ResolvedUnitType());
  }
  return To<CSSNumericLiteralValue>(value);
}

double ResolveColorChannelForNumericLiteral(
    const CSSNumericLiteralValue& value,
    ColorFunctionParser::ChannelType channel_type,
    double percentage_base) {
  using ChannelType = ColorFunctionParser::ChannelType;
  switch (channel_type) {
    case ChannelType::kNumber:
      if (value.IsAngle()) {
        return value.ComputeDegrees();
      } else {
        return value.DoubleValue();
      }
    case ChannelType::kPercentage:
      return (value.ClampedDoubleValue() / 100.0) * percentage_base;
    case ChannelType::kRelative:
    case ChannelType::kNone:
      NOTREACHED();
  }
}

double ResolveAlphaForNumericLiteral(
    const CSSNumericLiteralValue& value,
    ColorFunctionParser::ChannelType channel_type) {
  using ChannelType = ColorFunctionParser::ChannelType;
  switch (channel_type) {
    case ChannelType::kNumber:
      return ClampTo<double>(value.ClampedDoubleValue(), 0.0, 1.0);
    case ChannelType::kPercentage:
      return ClampTo<double>(value.ClampedDoubleValue() / 100.0, 0.0, 1.0);
    case ChannelType::kRelative:
    case ChannelType::kNone:
      NOTREACHED();
  }
}

}  // namespace

bool ColorFunctionParser::IsRelativeColor() const {
  return !!unresolved_origin_color_;
}

static bool ChannelIsResolvable(const CSSValue* channel,
                                Color::ColorSpace color_space) {
  if (IsA<CSSIdentifierValue>(channel)) {
    // Channel identifiers for relative colors (e.g. “r”).
    return true;
  }

  if (IsA<CSSNumericLiteralValue>(channel)) {
    // Numeric literals.
    return true;
  }

  // Lab/OkLab/Lch/OkLch preserve calc().
  if (color_space == Color::ColorSpace::kLab ||
      color_space == Color::ColorSpace::kOklab ||
      color_space == Color::ColorSpace::kLch ||
      color_space == Color::ColorSpace::kOklch) {
    return false;
  }

  const CSSMathFunctionValue* calc = DynamicTo<CSSMathFunctionValue>(channel);
  if (calc && calc->ExpressionNode()->IsNumericLiteral()) {
    // calc() of a single value. We wouldn't technically need
    // to accept this as a special case, but it _is_ resolvable,
    // and doing this allows us to hit the less-compliant code
    // in CSSUnresolvedColorValue::CustomCSSText() much less often.
    return true;
  }

  return false;
}

bool ColorFunctionParser::AllChannelsAreResolvable() const {
  for (int i = 0; i < 3; i++) {
    if (!ChannelIsResolvable(unresolved_channels_[i], color_space_)) {
      return false;
    }
  }

  if (unresolved_alpha_) {
    return ChannelIsResolvable(unresolved_alpha_, color_space_);
  }

  return true;
}

CSSValue* ColorFunctionParser::ConsumeFunctionalSyntaxColor(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const css_parsing_utils::ColorParserContext& color_parser_context) {
  CSSValueID function_id = stream.Peek().FunctionId();
  if (!IsValidColorFunction(function_id)) {
    return nullptr;
  }

  if (function_id == CSSValueID::kColor) {
    context.Count(WebFeature::kCSSColorFunction);
  }

  bool has_alpha = false;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (!ConsumeColorSpaceAndOriginColor(stream, function_id, context,
                                         color_parser_context)) {
      return nullptr;
    }

    // Parse the three color channel params.
    for (int i = 0; i < 3; i++) {
      if (!ConsumeChannel(stream, context, i, color_parser_context)) {
        return nullptr;
      }
      // Potentially expect a separator after the first and second channel. The
      // separator for a potential alpha channel is handled below.
      if (i < 2) {
        const bool matched_comma =
            css_parsing_utils::ConsumeCommaIncludingWhitespace(stream);
        if (is_legacy_syntax_) {
          // We've parsed one separating comma token, so we expect the second
          // separator to match.
          if (!matched_comma) {
            return nullptr;
          }
        } else if (matched_comma) {
          if (IsRelativeColor()) {
            return nullptr;
          }
          is_legacy_syntax_ = true;
        }
      }
    }

    // Parse alpha.
    if (is_legacy_syntax_) {
      if (!Color::IsLegacyColorSpace(color_space_)) {
        return nullptr;
      }
      // , <alpha-value>?
      if (css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
        has_alpha = true;
      }
    } else {
      // / <alpha-value>?
      if (css_parsing_utils::ConsumeSlashIncludingWhitespace(stream)) {
        has_alpha = true;
      }
    }
    if (has_alpha) {
      if (!ConsumeAlpha(stream, context, color_parser_context)) {
        return nullptr;
      }
    }

    if (!stream.AtEnd()) {
      return nullptr;
    }

    if (is_legacy_syntax_) {
      // "None" is not a part of the legacy syntax.
      if (has_none_) {
        return nullptr;
      }
      // Legacy rgb needs percentage consistency. Percentages need to be mapped
      // from the range [0, 1] to the [0, 255] that the color space uses.
      // Percentages and bare numbers CAN be mixed in relative colors.
      if (color_space_ == Color::ColorSpace::kSRGBLegacy) {
        bool uses_percentage = false;
        bool uses_bare_numbers = false;
        for (int i = 0; i < 3; i++) {
          if (channel_types_[i] == ChannelType::kNone) {
            continue;
          }
          if (channel_types_[i] == ChannelType::kPercentage) {
            if (uses_bare_numbers) {
              return nullptr;
            }
            uses_percentage = true;
          } else if (channel_types_[i] == ChannelType::kNumber) {
            if (uses_percentage) {
              return nullptr;
            }
            uses_bare_numbers = true;
          }
        }
      }

      // Legacy syntax is not allowed for hwb().
      if (color_space_ == Color::ColorSpace::kHWB) {
        return nullptr;
      }

      if (color_space_ == Color::ColorSpace::kHSL ||
          color_space_ == Color::ColorSpace::kHWB) {
        for (int i : {1, 2}) {
          if (channel_types_[i] == ChannelType::kNumber) {
            // Legacy color syntax needs percentages.
            return nullptr;
          }
        }
      }
    }

    // The parsing was successful, so we need to consume the input.
    guard.Release();
  }
  stream.ConsumeWhitespace();

  // For non-relative colors, resolve channel values at parse time.
  // For relative colors, always defer resolution until used-value time.
  std::optional<Color> resolved_color;
  if (!IsRelativeColor() && AllChannelsAreResolvable()) {
    // Resolve channel values.
    std::array<std::optional<double>, 3> channels;
    for (int i = 0; i < 3; i++) {
      if (channel_types_[i] != ChannelType::kNone) {
        channels[i] = ResolveColorChannelForNumericLiteral(
            GetNumericLiteralValue(*unresolved_channels_[i]), channel_types_[i],
            function_metadata_->channel_percentage[i]);
      }
    }

    std::optional<double> alpha = 1.0;
    if (has_alpha) {
      if (alpha_channel_type_ != ChannelType::kNone) {
        alpha = ResolveAlphaForNumericLiteral(
            GetNumericLiteralValue(*unresolved_alpha_), alpha_channel_type_);
      } else {
        alpha.reset();
      }
    }

    MakePerColorSpaceAdjustments(/*is_relative_color=*/false, is_legacy_syntax_,
                                 color_space_, channels, alpha);

    resolved_color = Color::FromColorSpace(color_space_, channels[0],
                                           channels[1], channels[2], alpha);
  }

  if (IsRelativeColor()) {
    context.Count(WebFeature::kCSSRelativeColor);
  } else {
    switch (color_space_) {
      case Color::ColorSpace::kSRGB:
      case Color::ColorSpace::kSRGBLinear:
      case Color::ColorSpace::kDisplayP3:
      case Color::ColorSpace::kDisplayP3Linear:
      case Color::ColorSpace::kA98RGB:
      case Color::ColorSpace::kProPhotoRGB:
      case Color::ColorSpace::kRec2020:
      case Color::ColorSpace::kRec2100Linear:
        context.Count(WebFeature::kCSSColor_SpaceRGB);
        if (resolved_color.has_value() && !IsInGamutRec2020(*resolved_color)) {
          context.Count(WebFeature::kCSSColor_SpaceRGB_outOfRec2020);
        }
        break;
      case Color::ColorSpace::kOklab:
      case Color::ColorSpace::kOklch:
        context.Count(WebFeature::kCSSColor_SpaceOkLxx);
        if (resolved_color.has_value() && !IsInGamutRec2020(*resolved_color)) {
          context.Count(WebFeature::kCSSColor_SpaceOkLxx_outOfRec2020);
        }
        break;
      case Color::ColorSpace::kLab:
      case Color::ColorSpace::kLch:
        context.Count(WebFeature::kCSSColor_SpaceLxx);
        break;
      case Color::ColorSpace::kHWB:
        context.Count(WebFeature::kCSSColor_SpaceHwb);
        break;
      case Color::ColorSpace::kXYZD50:
      case Color::ColorSpace::kXYZD65:
      case Color::ColorSpace::kSRGBLegacy:
      case Color::ColorSpace::kHSL:
      case Color::ColorSpace::kNone:
        break;
    }
  }

  if (resolved_color.has_value()) {
    return cssvalue::CSSColor::Create(*resolved_color);
  } else if (unresolved_origin_color_) {
    return MakeGarbageCollected<cssvalue::CSSRelativeColorValue>(
        *unresolved_origin_color_, color_space_, *unresolved_channels_[0],
        *unresolved_channels_[1], *unresolved_channels_[2], unresolved_alpha_);
  } else {
    if (!has_alpha) {
      unresolved_alpha_ = CSSNumericLiteralValue::Create(
          1.0, CSSNumericLiteralValue::UnitType::kNumber);
      alpha_channel_type_ = ChannelType::kNumber;
    }

    return MakeGarbageCollected<cssvalue::CSSUnresolvedColorValue>(
        color_space_, DynamicTo<CSSPrimitiveValue>(*unresolved_channels_[0]),
        DynamicTo<CSSPrimitiveValue>(*unresolved_channels_[1]),
        DynamicTo<CSSPrimitiveValue>(*unresolved_channels_[2]), channel_types_,
        DynamicTo<CSSPrimitiveValue>(unresolved_alpha_), alpha_channel_type_);
  }
}

}  // namespace blink
