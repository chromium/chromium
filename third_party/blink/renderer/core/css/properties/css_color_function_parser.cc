// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_color_function_parser.h"
#include <cmath>
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"

namespace blink {

static Color::ColorSpace CSSValueIDToColorSpace(const CSSValueID& id) {
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

static bool ColorChannelIsHue(Color::ColorSpace color_space, int channel) {
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

// https://www.w3.org/TR/css-color-5/#relative-colors
// e.g. lab(from magenta l a b), consume the "magenta" after the from. The
// result needs to be a blink::Color as we need actual values for the color
// parameters.
static bool ConsumeRelativeOriginColor(CSSParserTokenRange& args,
                                       const CSSParserContext& context,
                                       Color& result) {
  if (!RuntimeEnabledFeatures::CSSRelativeColorEnabled()) {
    return false;
  }
  if (CSSValue* css_color = css_parsing_utils::ConsumeColor(args, context)) {
    if (auto* color_value = DynamicTo<cssvalue::CSSColor>(css_color)) {
      result = color_value->Value();
      return true;
    } else {
      CSSValueID value_id = To<CSSIdentifierValue>(css_color)->GetValueID();
      // TODO(crbug.com/1447327): Just like with
      // css_parsing_utils::ResolveColor(), currentcolor is not currently
      // handled.
      if (value_id == CSSValueID::kCurrentcolor) {
        return false;
      }
      // TODO(crbug.com/1447327): Handle color scheme.
      result = StyleColor::ColorFromKeyword(value_id,
                                            mojom::blink::ColorScheme::kLight);
      return true;
    }
  }
  return false;
}

static absl::optional<double> ConsumeRelativeColorChannel(
    CSSParserTokenRange& input_range,
    const CSSParserContext& context,
    const HashMap<CSSValueID, double> color_channel_keyword_values) {
  const CSSParserToken& token = input_range.Peek();
  // Relative color channels can be calc() functions with color channel
  // replacements. e.g. In "color(from magenta srgb calc(r / 2) 0 0)", the
  // "calc" should substitute "1" for "r" (magenta has a full red channel).
  if (token.GetType() == kFunctionToken) {
    // Don't consume the range if the parsing fails.
    CSSParserTokenRange calc_range = input_range;
    CSSMathFunctionValue* calc_value = CSSMathFunctionValue::Create(
        CSSMathExpressionNode::ParseMathFunction(
            token.FunctionId(), css_parsing_utils::ConsumeFunction(calc_range),
            context, true /* is_percentage_allowed */, kCSSAnchorQueryTypesNone,
            color_channel_keyword_values),
        CSSPrimitiveValue::ValueRange::kAll);
    if (calc_value) {
      if (calc_value->Category() != kCalcNumber) {
        return absl::nullopt;
      }
      // Consume the range, since it has succeeded.
      input_range = calc_range;
      return calc_value->GetDoubleValueWithoutClamping();
    }
  }

  // This is for just single variable swaps without calc(). e.g. The "l" in
  // "lab(from cyan l 0.5 0.5)".
  if (color_channel_keyword_values.Contains(token.Id())) {
    input_range.ConsumeIncludingWhitespace();
    return color_channel_keyword_values.at(token.Id());
  }

  return absl::nullopt;
}

// Relative color syntax requires "channel keyword" substitutions for color
// channels. Each color space has three "channel keywords", plus "alpha", that
// correspond to the three parameters stored on the origin color. This function
// generates a map between the channel keywords and the stored values in order
// to make said substitutions. e.g. color(from magenta srgb r g b) will need to
// generate srgb keyword values for the origin color "magenta". This function
// will return: {CSSValueID::kR: 1, CSSValueID::kG: 0, CSSValueID::kB: 1}.
static HashMap<CSSValueID, double> GenerateChannelKeywordValues(
    Color::ColorSpace color_space,
    Color origin_color) {
  std::vector<CSSValueID> channel_names;
  switch (color_space) {
    case Color::ColorSpace::kSRGB:
    case Color::ColorSpace::kSRGBLinear:
    case Color::ColorSpace::kSRGBLegacy:
    case Color::ColorSpace::kDisplayP3:
    case Color::ColorSpace::kA98RGB:
    case Color::ColorSpace::kProPhotoRGB:
    case Color::ColorSpace::kRec2020:
    case Color::ColorSpace::kXYZD50:
    case Color::ColorSpace::kXYZD65:
      channel_names = {CSSValueID::kR, CSSValueID::kG, CSSValueID::kB};
      break;
    case Color::ColorSpace::kLab:
    case Color::ColorSpace::kOklab:
      channel_names = {CSSValueID::kL, CSSValueID::kA, CSSValueID::kB};
      break;
    case Color::ColorSpace::kLch:
    case Color::ColorSpace::kOklch:
      channel_names = {CSSValueID::kL, CSSValueID::kC, CSSValueID::kH};
      break;
    case Color::ColorSpace::kHSL:
      channel_names = {CSSValueID::kH, CSSValueID::kS, CSSValueID::kL};
      break;
    case Color::ColorSpace::kHWB:
      channel_names = {CSSValueID::kH, CSSValueID::kW, CSSValueID::kB};
      break;
    case Color::ColorSpace::kNone:
      NOTREACHED();
      break;
  }

  return {
      {channel_names[0], origin_color.Param0()},
      {channel_names[1], origin_color.Param1()},
      {channel_names[2], origin_color.Param2()},
      {CSSValueID::kAlpha, origin_color.Alpha()},
  };
}

bool ColorFunctionParser::ConsumeColorSpaceAndOriginColor(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSParserTokenRange& args) {
  // Get the color space. This will either be the name of the function, or it
  // will be the first argument of the "color" function.
  CSSValueID function_id = range.Peek().FunctionId();
  color_space_ = CSSValueIDToColorSpace(function_id);
  if (color_space_ == Color::ColorSpace::kNone &&
      function_id != CSSValueID::kColor) {
    return false;
  }
  args = css_parsing_utils::ConsumeFunction(range);

  // This is in the form color(COLOR_SPACE r g b)
  if (function_id == CSSValueID::kColor) {
    if (css_parsing_utils::ConsumeIdent<CSSValueID::kFrom>(args)) {
      if (!ConsumeRelativeOriginColor(args, context, origin_color_)) {
        return false;
      }
      is_relative_color_ = true;
    }
    color_space_ =
        CSSValueIDToColorSpace(args.ConsumeIncludingWhitespace().Id());
    if (!Color::IsPredefinedColorSpace(color_space_)) {
      return false;
    }
  }

  if (css_parsing_utils::ConsumeIdent<CSSValueID::kFrom>(args)) {
    // Can't have more than one "from" in a single color.
    // Relative color is invalid for rgba()/hsla functions
    if (is_relative_color_ || function_id == CSSValueID::kRgba ||
        function_id == CSSValueID::kHsla ||
        !ConsumeRelativeOriginColor(args, context, origin_color_)) {
      return false;
    }
    is_relative_color_ = true;
  }

  if (is_relative_color_) {
    origin_color_.ConvertToColorSpace(color_space_);
    channel_keyword_values_ =
        GenerateChannelKeywordValues(color_space_, origin_color_);
    if (Color::IsPredefinedColorSpace(color_space_)) {
      // Relative colors with color() can use 'x', 'y', 'z' in the place of 'r',
      // 'g', 'b'.
      xyz_keyword_values_ = {
          {CSSValueID::kX, origin_color_.Param0()},
          {CSSValueID::kY, origin_color_.Param1()},
          {CSSValueID::kZ, origin_color_.Param2()},
          {CSSValueID::kAlpha, origin_color_.Alpha()},
      };
    }
  }

  return true;
}

// ConsumeHue takes an angle as input (as angle in radians or in degrees, or as
// plain number in degrees) and returns a plain number in degrees.
static absl::optional<double> ConsumeHue(CSSParserTokenRange& range,
                                         const CSSParserContext& context) {
  CSSPrimitiveValue* value =
      css_parsing_utils::ConsumeAngle(range, context, absl::nullopt);
  double angle_value;
  if (!value) {
    value = css_parsing_utils::ConsumeNumber(
        range, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!value) {
      return absl::nullopt;
    }
    angle_value = value->GetDoubleValueWithoutClamping();
  } else {
    angle_value = value->ComputeDegrees();
  }
  return angle_value;
}

bool ColorFunctionParser::ConsumeChannel(CSSParserTokenRange& args,
                                         const CSSParserContext& context,
                                         int i) {
  CSSPrimitiveValue* temp;
  if (is_legacy_syntax_ &&
      !css_parsing_utils::ConsumeCommaIncludingWhitespace(args)) {
    // Commas must be consistent.
    return false;
  }
  if (css_parsing_utils::ConsumeCommaIncludingWhitespace(args)) {
    if (is_relative_color_) {
      return false;
    }
    is_legacy_syntax_ = true;
  }
  if (css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(args)) {
    channel_types_[i] = ChannelType::kNone;
    has_none_ = true;
    return true;
  }

  if (ColorChannelIsHue(color_space_, i)) {
    if ((channels_[i] = ConsumeHue(args, context))) {
      channel_types_[i] = ChannelType::kNumber;
    } else if (is_relative_color_ &&
               (channels_[i] = ConsumeRelativeColorChannel(
                    args, context, channel_keyword_values_))) {
      channel_types_[i] = ChannelType::kRelative;
    }

    if (!channels_[i].has_value()) {
      return false;
    }

    // Non-finite values should be clamped to the range [0, 360].
    // Since 0 = 360 in this case, they can all simply become zero.
    if (!isfinite(channels_[i].value())) {
      channels_[i] = 0.0;
    }

    // Wrap hue to be in the range [0, 360].
    channels_[i].value() =
        fmod(fmod(channels_[i].value(), 360.0) + 360.0, 360.0);
    return true;
  }

  if ((temp = css_parsing_utils::ConsumeNumber(
           args, context, CSSPrimitiveValue::ValueRange::kAll))) {
    channels_[i] = temp->GetDoubleValueWithoutClamping();
    channel_types_[i] = ChannelType::kNumber;
    return true;
  }

  if ((temp = css_parsing_utils::ConsumePercent(
           args, context, CSSPrimitiveValue::ValueRange::kAll))) {
    channels_[i] = temp->GetDoubleValue() / 100.0;
    channel_types_[i] = ChannelType::kPercentage;
    return true;
  }

  if (is_relative_color_) {
    channel_types_[i] = ChannelType::kRelative;
    // First, check if the channel contains only the keyword "alpha", because
    // that can be either an rgb or an xyz param.
    if ((channels_[i] = ConsumeRelativeColorChannel(
             args, context, {{CSSValueID::kAlpha, origin_color_.Alpha()}}))) {
      return true;
    }
    if ((channels_[i] = ConsumeRelativeColorChannel(args, context,
                                                    channel_keyword_values_))) {
      uses_rgb_relative_params_ = true;
      return true;
    }

    if (Color::IsPredefinedColorSpace(color_space_) &&
        (channels_[i] =
             ConsumeRelativeColorChannel(args, context, xyz_keyword_values_))) {
      uses_xyz_relative_params_ = true;
      return true;
    }
  }

  // Missing components should not parse.
  return false;
}

bool ColorFunctionParser::ConsumeAlpha(CSSParserTokenRange& args,
                                       const CSSParserContext& context) {
  CSSPrimitiveValue* temp;
  if ((temp = css_parsing_utils::ConsumeNumber(
           args, context, CSSPrimitiveValue::ValueRange::kAll))) {
    alpha_ = ClampTo<double>(temp->GetDoubleValue(), 0.0, 1.0);
    return true;
  }

  if ((temp = css_parsing_utils::ConsumePercent(
           args, context, CSSPrimitiveValue::ValueRange::kAll))) {
    alpha_ = ClampTo<double>(temp->GetDoubleValue() / 100.0, 0.0, 1.0);
    return true;
  }

  if (css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(args)) {
    has_none_ = true;
    alpha_.reset();
    return true;
  }

  if ((alpha_ = ConsumeRelativeColorChannel(
           args, context, {{CSSValueID::kAlpha, origin_color_.Alpha()}}))) {
    // Same as above, check if the channel contains only the keyword
    // "alpha", because that can be either an rgb or an xyz param.
    return true;
  }

  if (is_relative_color_ && (alpha_ = ConsumeRelativeColorChannel(
                                 args, context, channel_keyword_values_))) {
    uses_rgb_relative_params_ = true;
    return true;
  }

  if (is_relative_color_ && Color::IsPredefinedColorSpace(color_space_) &&
      (alpha_ =
           ConsumeRelativeColorChannel(args, context, xyz_keyword_values_))) {
    uses_xyz_relative_params_ = true;
    return true;
  }

  return false;
}

bool ColorFunctionParser::MakePerColorSpaceAdjustments() {
  // Legacy rgb needs percentage consistency. Non-percentages need to be mapped
  // from the range [0, 255] to the [0, 1] that we store internally.
  // Percentages and bare numbers CAN be mixed in relative colors.
  if (color_space_ == Color::ColorSpace::kSRGBLegacy) {
    bool uses_percentage = false;
    bool uses_bare_numbers = false;
    for (int i = 0; i < 3; i++) {
      if (channel_types_[i] == ChannelType::kNone) {
        continue;
      }
      if (channel_types_[i] == ChannelType::kPercentage) {
        if (uses_bare_numbers && !is_relative_color_) {
          return false;
        }
        uses_percentage = true;
      } else if (channel_types_[i] == ChannelType::kNumber) {
        if (uses_percentage && !is_relative_color_) {
          return false;
        }
        uses_bare_numbers = true;
        channels_[i].value() /= 255.0;
      }

      if (!isfinite(channels_[i].value())) {
        channels_[i].value() = channels_[i].value() > 0 ? 1 : 0;
      } else if (!is_relative_color_) {
        // Clamp to [0, 1] range, but allow out-of-gamut relative colors.
        channels_[i].value() = ClampTo<double>(channels_[i].value(), 0.0, 1.0);
      }
    }
    // TODO(crbug.com/1399566): There are many code paths that still compress
    // alpha to be an 8-bit integer. If it is not explicitly compressed here,
    // tests will fail due to some paths doing this compression and others not.
    // See compositing/background-color/background-color-alpha.html for example.
    // Ideally we would allow alpha to be any float value, but we have to clean
    // up all spots where this compression happens before this is possible.
    if (!is_relative_color_ && alpha_.has_value()) {
      alpha_ = round(alpha_.value() * 255.0) / 255.0;
    }
  }

    // Legacy syntax is not allowed for hwb().
  if (color_space_ == Color::ColorSpace::kHWB && is_legacy_syntax_) {
    return false;
  }

  if (color_space_ == Color::ColorSpace::kHSL ||
      color_space_ == Color::ColorSpace::kHWB) {
    for (int i : {1, 2}) {
      if (channel_types_[i] == ChannelType::kNumber) {
        // Legacy color syntax needs percentages.
        if (is_legacy_syntax_) {
          return false;
        }
        // Raw numbers are interpreted as percentages in these color spaces.
        channels_[i] = channels_[i].value() / 100.0;
      }
      if (channels_[i].has_value() && is_legacy_syntax_) {
        channels_[i] = ClampTo<double>(channels_[i].value(), 0.0, 1.0);
      }
    }
  }

  if (Color::IsLightnessFirstComponent(color_space_)) {
    // "Lightness" (param0) for lab/lch is in the range [0, 100], with 100%
    // corresponding to 100. "Lightness" (param0) for oklab/oklch is in the
    // range [0, 1], with 100% corresponding to 1. This means that we can just
    // take the numbers as input, with the exception that percentages for
    // lab/lch must be multiplied by 100.
    if (channel_types_[0] == ChannelType::kPercentage &&
        (color_space_ == Color::ColorSpace::kLab ||
         color_space_ == Color::ColorSpace::kLch)) {
      channels_[0].value() *= 100.0;
    }

    // For lab() and oklab() percentage inputs for a or b need to be mapped onto
    // the correct ranges. https://www.w3.org/TR/css-color-4/#specifying-lab-lch
    if (!Color::IsChromaSecondComponent(color_space_)) {
      const double ab_coefficient_for_percentages =
          (color_space_ == Color::ColorSpace::kLab) ? 125 : 0.4;

      if (channel_types_[1] == ChannelType::kPercentage) {
        channels_[1].value() *= ab_coefficient_for_percentages;
      }
      if (channel_types_[2] == ChannelType::kPercentage) {
        channels_[2].value() *= ab_coefficient_for_percentages;
      }
    } else {
      // Same as above, mapping percentage values for chroma in lch()/oklch().
      const double chroma_coefficient_for_percentages =
          (color_space_ == Color::ColorSpace::kLch) ? 150 : 0.4;
      if (channel_types_[1] == ChannelType::kPercentage) {
        channels_[1].value() *= chroma_coefficient_for_percentages;
      }
    }
  }

  return true;
}

bool ColorFunctionParser::ConsumeFunctionalSyntaxColor(
    CSSParserTokenRange& input_range,
    const CSSParserContext& context,
    Color& result) {
  // Copy the range so that it is not consumed if the parsing fails.
  CSSParserTokenRange range = input_range;
  CSSParserTokenRange args = range;

  if (!ConsumeColorSpaceAndOriginColor(range, context, args)) {
    return false;
  }

  // Parse the three color channel params.
  for (int i = 0; i < 3; i++) {
    if (!ConsumeChannel(args, context, i)) {
      return false;
    }
  }

  // Parse alpha.
  bool expect_alpha = false;
  if (css_parsing_utils::ConsumeSlashIncludingWhitespace(args)) {
    expect_alpha = true;
    if (is_legacy_syntax_) {
      return false;
    }
  } else if (Color::IsLegacyColorSpace(color_space_) && is_legacy_syntax_ &&
             css_parsing_utils::ConsumeCommaIncludingWhitespace(args)) {
    expect_alpha = true;
  }
  if (expect_alpha && !ConsumeAlpha(args, context)) {
    return false;
  }
  if (!expect_alpha && is_relative_color_) {
    alpha_ = channel_keyword_values_.at(CSSValueID::kAlpha);
  }

  // Cannot mix the two color channel keyword types.
  // "None" is not a part of the legacy syntax.
  if (!args.AtEnd() ||
      (uses_rgb_relative_params_ && uses_xyz_relative_params_) ||
      (is_legacy_syntax_ && has_none_)) {
    return false;
  }

  if (!MakePerColorSpaceAdjustments()) {
    return false;
  }

  result = Color::FromColorSpace(color_space_, channels_[0], channels_[1],
                                 channels_[2], alpha_);
  if (is_relative_color_ && Color::IsLegacyColorSpace(color_space_)) {
    result.ConvertToColorSpace(Color::ColorSpace::kSRGB);
  }
  // The parsing was successful, so we need to consume the input.
  input_range = range;

  if (is_relative_color_) {
    context.Count(WebFeature::kCSSRelativeColor);
  }

  return true;
}

}  // namespace blink
