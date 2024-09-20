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
    case CSSValueID::kSRGBLinear:
      return Color::ColorSpace::kSRGBLinear;
    case CSSValueID::kDisplayP3:
      return Color::ColorSpace::kDisplayP3;
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

// If the CSSValue is an absolute color, return the corresponding Color.
std::optional<Color> TryResolveAtParseTime(const CSSValue& value) {
  if (auto* color_value = DynamicTo<cssvalue::CSSColor>(value)) {
    return color_value->Value();
  }
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    // We can resolve <named-color> and 'transparent' at parse-time.
    CSSValueID value_id = identifier_value->GetValueID();
    if ((value_id >= CSSValueID::kAqua && value_id <= CSSValueID::kYellow) ||
        (value_id >= CSSValueID::kAliceblue &&
         value_id <= CSSValueID::kYellowgreen) ||
        value_id == CSSValueID::kTransparent || value_id == CSSValueID::kGrey) {
      // We're passing 'light' as the color-scheme, but nothing above should
      // depend on that value (i.e it's a dummy argument). Ditto for the null
      // color provider.
      return StyleColor::ColorFromKeyword(
          value_id, mojom::blink::ColorScheme::kLight, nullptr,
          /*is_in_web_app_scope=*/false);
    }
    return std::nullopt;
  }
  if (auto* color_mix_value = DynamicTo<cssvalue::CSSColorMixValue>(value)) {
    auto color1 = TryResolveAtParseTime(color_mix_value->Color1());
    auto color2 = TryResolveAtParseTime(color_mix_value->Color2());
    if (!color1 || !color2) {
      return std::nullopt;
    }
    // We can only mix with percentages being numeric literals from here,
    // as we don't have a length conversion data to resolve against yet.
    if ((!color_mix_value->Percentage1() ||
         color_mix_value->Percentage1()->IsNumericLiteralValue()) &&
        (!color_mix_value->Percentage2() ||
         color_mix_value->Percentage2()->IsNumericLiteralValue())) {
      return color_mix_value->Mix(*color1, *color2,
                                  CSSToLengthConversionData());
    }
  }
  if (auto* relative_color_value =
          DynamicTo<cssvalue::CSSRelativeColorValue>(value)) {
    auto origin_color =
        TryResolveAtParseTime(relative_color_value->OriginColor());
    if (!origin_color) {
      return std::nullopt;
    }
    StyleColor::UnresolvedRelativeColor* unresolved_relative_color =
        MakeGarbageCollected<StyleColor::UnresolvedRelativeColor>(
            StyleColor(origin_color.value()),
            relative_color_value->ColorInterpolationSpace(),
            relative_color_value->Channel0(), relative_color_value->Channel1(),
            relative_color_value->Channel2(), relative_color_value->Alpha());
    return unresolved_relative_color->Resolve(Color());
  }
  return std::nullopt;
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
    const CSSParserContext& context) {
  // [from <color>]?
  if (css_parsing_utils::ConsumeIdent<CSSValueID::kFrom>(stream)) {
    if (!RuntimeEnabledFeatures::CSSRelativeColorEnabled()) {
      return false;
    }
    unresolved_origin_color_ = css_parsing_utils::ConsumeColor(stream, context);
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
    origin_color_ = TryResolveAtParseTime(*unresolved_origin_color_);
    if (origin_color_.has_value() &&
        !RuntimeEnabledFeatures::CSSRelativeColorLateResolveAlwaysEnabled()) {
      origin_color_->ConvertToColorSpace(color_space_);
      // Relative color syntax requires "channel keyword" substitutions for
      // color channels. Each color space has three "channel keywords", plus
      // "alpha", that correspond to the three parameters stored on the origin
      // color. This function generates a map between the channel keywords and
      // the stored values in order to make said substitutions. e.g. color(from
      // magenta srgb r g b) will need to generate srgb keyword values for the
      // origin color "magenta". This will produce a map like: {CSSValueID::kR:
      // 1, CSSValueID::kG: 0, CSSValueID::kB: 1, CSSValueID::kAlpha: 1}.
      std::array<double, 3> channel_values = {origin_color_->Param0(),
                                              origin_color_->Param1(),
                                              origin_color_->Param2()};

      // Convert from the [0 1] range to the [0 100] range for hsl() and
      // hwb(). This is the inverse of the transform in
      // MakePerColorSpaceAdjustments().
      if (color_space_ == Color::ColorSpace::kHSL ||
          color_space_ == Color::ColorSpace::kHWB) {
        channel_values[1] *= 100;
        channel_values[2] *= 100;
      }

      color_channel_map_ = {
          {function_metadata_->channel_name[0], channel_values[0]},
          {function_metadata_->channel_name[1], channel_values[1]},
          {function_metadata_->channel_name[2], channel_values[2]},
          {CSSValueID::kAlpha, origin_color_->Alpha()},
      };
    } else {
      if (!origin_color_.has_value() &&
          !RuntimeEnabledFeatures::
              CSSRelativeColorSupportsCurrentcolorEnabled()) {
        return false;
      }
      // If the origin color is not resolvable at parse time, fill out the map
      // with just the valid channel names. We still need that information to
      // parse the remainder of the color function.
      color_channel_map_ = {
          {function_metadata_->channel_name[0], std::nullopt},
          {function_metadata_->channel_name[1], std::nullopt},
          {function_metadata_->channel_name[2], std::nullopt},
          {CSSValueID::kAlpha, std::nullopt},
      };
    }
  }
  return true;
}

bool ColorFunctionParser::ConsumeChannel(CSSParserTokenStream& stream,
                                         const CSSParserContext& context,
                                         int i) {
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

    return true;
  }

  if ((unresolved_channels_[i] = css_parsing_utils::ConsumeNumber(
           stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
    channel_types_[i] = ChannelType::kNumber;
    return true;
  }

  if ((unresolved_channels_[i] = css_parsing_utils::ConsumePercent(
           stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
    channel_types_[i] = ChannelType::kPercentage;
    return true;
  }

  if (IsRelativeColor()) {
    channel_types_[i] = ChannelType::kRelative;
    if ((unresolved_channels_[i] = ConsumeRelativeColorChannel(
             stream, context, color_channel_map_, {kCalcNumber, kCalcPercent},
             function_metadata_->channel_percentage[i]))) {
      return true;
    }
  }

  // Missing components should not parse.
  return false;
}

bool ColorFunctionParser::ConsumeAlpha(CSSParserTokenStream& stream,
                                       const CSSParserContext& context) {
  if ((unresolved_alpha_ = css_parsing_utils::ConsumeNumber(
           stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
    alpha_channel_type_ = ChannelType::kNumber;
    return true;
  }

  if ((unresolved_alpha_ = css_parsing_utils::ConsumePercent(
           stream, context, CSSPrimitiveValue::ValueRange::kAll))) {
    alpha_channel_type_ = ChannelType::kPercentage;
    return true;
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
    return true;
  }

  return false;
}

void ColorFunctionParser::MakePerColorSpaceAdjustments() {
  if (color_space_ == Color::ColorSpace::kSRGBLegacy) {
    for (int i = 0; i < 3; i++) {
      if (channel_types_[i] == ChannelType::kNone) {
        continue;
      }
      if (!isfinite(channels_[i].value())) {
        channels_[i].value() = channels_[i].value() > 0 ? 255.0 : 0;
      } else if (!IsRelativeColor()) {
        // Clamp to [0, 1] range, but allow out-of-gamut relative colors.
        channels_[i].value() =
            ClampTo<double>(channels_[i].value(), 0.0, 255.0);
      }
    }
    // TODO(crbug.com/1399566): There are many code paths that still compress
    // alpha to be an 8-bit integer. If it is not explicitly compressed here,
    // tests will fail due to some paths doing this compression and others not.
    // See compositing/background-color/background-color-alpha.html for example.
    // Ideally we would allow alpha to be any float value, but we have to clean
    // up all spots where this compression happens before this is possible.
    if (!IsRelativeColor() && alpha_.has_value()) {
      alpha_ = round(alpha_.value() * 255.0) / 255.0;
    }
  }

  if (color_space_ == Color::ColorSpace::kHSL ||
      color_space_ == Color::ColorSpace::kHWB) {
    for (int i : {1, 2}) {
      // Raw numbers are interpreted as percentages in these color spaces.
      if (channels_[i].has_value()) {
        channels_[i] = channels_[i].value() / 100.0;

        if (is_legacy_syntax_) {
          channels_[i] = ClampTo<double>(channels_[i].value(), 0.0, 1.0);
        }
      }
    }
  }
}

double ColorFunctionParser::ResolveColorChannel(
    const CSSValue* value,
    ChannelType channel_type,
    double percentage_base,
    const CSSColorChannelMap& color_channel_map) {
  if (const CSSPrimitiveValue* primitive_value =
          DynamicTo<CSSPrimitiveValue>(value)) {
    switch (channel_type) {
      case ChannelType::kNumber:
        if (primitive_value->IsAngle()) {
          return primitive_value->ComputeDegrees();
        } else {
          return primitive_value->GetDoubleValueWithoutClamping();
        }
      case ChannelType::kPercentage:
        return (primitive_value->GetDoubleValue() / 100.0) * percentage_base;
      case ChannelType::kRelative:
        // Proceed to relative channel value resolution below.
        break;
      default:
        NOTREACHED();
    }
  }

  return ResolveRelativeChannelValue(value, channel_type, percentage_base,
                                     color_channel_map);
}

double ColorFunctionParser::ResolveAlpha(
    const CSSValue* value,
    ChannelType channel_type,
    const CSSColorChannelMap& color_channel_map) {
  if (const CSSPrimitiveValue* primitive_value =
          DynamicTo<CSSPrimitiveValue>(value)) {
    switch (channel_type) {
      case ChannelType::kNumber:
        return ClampTo<double>(primitive_value->GetDoubleValue(), 0.0, 1.0);
      case ChannelType::kPercentage:
        return ClampTo<double>(primitive_value->GetDoubleValue() / 100.0, 0.0,
                               1.0);
      case ChannelType::kRelative:
        // Proceed to relative channel value resolution below.
        break;
      default:
        NOTREACHED();
    }
  }

  return ResolveRelativeChannelValue(
      value, channel_type, /*percentage_base=*/1.0, color_channel_map);
}

double ColorFunctionParser::ResolveRelativeChannelValue(
    const CSSValue* value,
    ChannelType channel_type,
    double percentage_base,
    const CSSColorChannelMap& color_channel_map) {
  if (const CSSIdentifierValue* identifier_value =
          DynamicTo<CSSIdentifierValue>(value)) {
    // This is for just single variable swaps without calc(). e.g. The "l" in
    // "lab(from cyan l 0.5 0.5)".
    if (auto it = color_channel_map.find(identifier_value->GetValueID());
        it != color_channel_map.end()) {
      return it->value.value();
    }
  }

  if (const CSSMathFunctionValue* calc_value =
          DynamicTo<CSSMathFunctionValue>(value)) {
    switch (calc_value->Category()) {
      case kCalcNumber:
        return calc_value->GetDoubleValueWithoutClamping();
      case kCalcPercent:
        return (calc_value->GetDoubleValue() / 100) * percentage_base;
      case kCalcAngle:
        return calc_value->ComputeDegrees();
      default:
        NOTREACHED();
    }
  }

  NOTREACHED();
}

bool ColorFunctionParser::IsRelativeColor() const {
  return !!unresolved_origin_color_;
}

CSSValue* ColorFunctionParser::ConsumeFunctionalSyntaxColor(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  CSSValueID function_id = stream.Peek().FunctionId();
  if (!IsValidColorFunction(function_id)) {
    return nullptr;
  }

  std::optional<Color> resolved_color;
  bool has_alpha = false;
  {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    if (!ConsumeColorSpaceAndOriginColor(stream, function_id, context)) {
      return nullptr;
    }

    // Parse the three color channel params.
    for (int i = 0; i < 3; i++) {
      if (!ConsumeChannel(stream, context, i)) {
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
      if (!ConsumeAlpha(stream, context)) {
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
  // For relative colors:
  // - (Legacy behavior) Resolve channel values at parse time if the origin
  //   color is resolvable at parse time.
  // - (WPT-compliant behavior) Always defer resolution until used-value time.
  if (!IsRelativeColor() ||
      (origin_color_.has_value() &&
       !RuntimeEnabledFeatures::CSSRelativeColorLateResolveAlwaysEnabled())) {
    // Resolve channel values.
    for (int i = 0; i < 3; i++) {
      if (channel_types_[i] != ChannelType::kNone) {
        channels_[i] = ResolveColorChannel(
            unresolved_channels_[i], channel_types_[i],
            function_metadata_->channel_percentage[i], color_channel_map_);

        if (ColorChannelIsHue(color_space_, i)) {
          // Non-finite values should be clamped to the range [0, 360].
          // Since 0 = 360 in this case, they can all simply become zero.
          if (!isfinite(channels_[i].value())) {
            channels_[i] = 0.0;
          }

          // Wrap hue to be in the range [0, 360].
          channels_[i].value() =
              fmod(fmod(channels_[i].value(), 360.0) + 360.0, 360.0);
        }
      }
    }

    if (has_alpha) {
      if (alpha_channel_type_ != ChannelType::kNone) {
        alpha_ = ResolveAlpha(unresolved_alpha_, alpha_channel_type_,
                              color_channel_map_);
      } else {
        alpha_.reset();
      }
    } else if (IsRelativeColor()) {
      alpha_ = color_channel_map_.at(CSSValueID::kAlpha);
    }

    MakePerColorSpaceAdjustments();

    resolved_color = Color::FromColorSpace(color_space_, channels_[0],
                                           channels_[1], channels_[2], alpha_);
    if (IsRelativeColor() && Color::IsLegacyColorSpace(color_space_)) {
      resolved_color->ConvertToColorSpace(Color::ColorSpace::kSRGB);
    }
  }

  if (IsRelativeColor()) {
    context.Count(WebFeature::kCSSRelativeColor);
  } else {
    switch (color_space_) {
      case Color::ColorSpace::kSRGB:
      case Color::ColorSpace::kSRGBLinear:
      case Color::ColorSpace::kDisplayP3:
      case Color::ColorSpace::kA98RGB:
      case Color::ColorSpace::kProPhotoRGB:
      case Color::ColorSpace::kRec2020:
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
      case Color::ColorSpace::kXYZD50:
      case Color::ColorSpace::kXYZD65:
      case Color::ColorSpace::kLab:
      case Color::ColorSpace::kLch:
      case Color::ColorSpace::kSRGBLegacy:
      case Color::ColorSpace::kHSL:
      case Color::ColorSpace::kHWB:
      case Color::ColorSpace::kNone:
        break;
    }
  }

  if (resolved_color.has_value()) {
    return cssvalue::CSSColor::Create(*resolved_color);
  } else {
    return MakeGarbageCollected<cssvalue::CSSRelativeColorValue>(
        *unresolved_origin_color_, color_space_, *unresolved_channels_[0],
        *unresolved_channels_[1], *unresolved_channels_[2], unresolved_alpha_);
  }
}

}  // namespace blink
