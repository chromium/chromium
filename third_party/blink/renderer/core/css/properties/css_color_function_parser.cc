// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_color_function_parser.h"

#include <cmath>

#include "base/containers/fixed_flat_map.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"

namespace blink {

struct ColorFunctionParser::FunctionMetadata {
  // The name/binding for positional color channels 0, 1 and 2.
  std::array<CSSValueID, 3> channel_name;

  // The value (number) that equals 100% for the corresponding positional color
  // channel.
  std::array<double, 3> channel_percentage;
};

namespace {

Color::ColorSpace CSSValueIDToColorSpace(const CSSValueID& id) {
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

// Unique entries in kFunctionMetadataMap.
enum class FunctionMetadataEntry : uint8_t {
  kLegacyRgb,  // Color::ColorSpace::kSRGBLegacy
  kColorRgb,   // Color::ColorSpace::kSRGB,
               // Color::ColorSpace::kSRGBLinear,
               // Color::ColorSpace::kDisplayP3,
               // Color::ColorSpace::kA98RGB,
               // Color::ColorSpace::kProPhotoRGB,
               // Color::ColorSpace::kRec2020
  kColorXyz,   // Color::ColorSpace::kXYZD50,
               // Color::ColorSpace::kXYZD65
  kLab,        // Color::ColorSpace::kLab
  kOkLab,      // Color::ColorSpace::kOklab
  kLch,        // Color::ColorSpace::kLch
  kOkLch,      // Color::ColorSpace::kOklch
  kHsl,        // Color::ColorSpace::kHSL
  kHwb,        // Color::ColorSpace::kHWB
};

constexpr double kPercentNotApplicable =
    std::numeric_limits<double>::quiet_NaN();

constexpr auto kFunctionMetadataMap =
    base::MakeFixedFlatMap<FunctionMetadataEntry,
                           ColorFunctionParser::FunctionMetadata>({
        // rgb(); percentage mapping: r,g,b=255
        {FunctionMetadataEntry::kLegacyRgb,
         {{CSSValueID::kR, CSSValueID::kG, CSSValueID::kB}, {255, 255, 255}}},

        // color(... <predefined-rgb-params> ...); percentage mapping: r,g,b=1
        {FunctionMetadataEntry::kColorRgb,
         {{CSSValueID::kR, CSSValueID::kG, CSSValueID::kB}, {1, 1, 1}}},

        // color(... <xyz-params> ...); percentage mapping: x,y,z=1
        {FunctionMetadataEntry::kColorXyz,
         {{CSSValueID::kX, CSSValueID::kY, CSSValueID::kZ}, {1, 1, 1}}},

        // lab(); percentage mapping: l=100 a,b=125
        {FunctionMetadataEntry::kLab,
         {{CSSValueID::kL, CSSValueID::kA, CSSValueID::kB}, {100, 125, 125}}},

        // oklab(); percentage mapping: l=1 a,b=0.4
        {FunctionMetadataEntry::kOkLab,
         {{CSSValueID::kL, CSSValueID::kA, CSSValueID::kB}, {1, 0.4, 0.4}}},

        // lch(); percentage mapping: l=100 c=150 h=n/a
        {FunctionMetadataEntry::kLch,
         {{CSSValueID::kL, CSSValueID::kC, CSSValueID::kH},
          {100, 150, kPercentNotApplicable}}},

        // oklch(); percentage mapping: l=1 c=0.4 h=n/a
        {FunctionMetadataEntry::kOkLch,
         {{CSSValueID::kL, CSSValueID::kC, CSSValueID::kH},
          {1, 0.4, kPercentNotApplicable}}},

        // hsl(); percentage mapping: h=n/a s,l=100
        {FunctionMetadataEntry::kHsl,
         {{CSSValueID::kH, CSSValueID::kS, CSSValueID::kL},
          {kPercentNotApplicable, 100, 100}}},

        // hwb(); percentage mapping: h=n/a w,b=100
        {FunctionMetadataEntry::kHwb,
         {{CSSValueID::kH, CSSValueID::kW, CSSValueID::kB},
          {kPercentNotApplicable, 100, 100}}},
    });

constexpr auto kColorSpaceFunctionMap =
    base::MakeFixedFlatMap<Color::ColorSpace, FunctionMetadataEntry>({
        {Color::ColorSpace::kSRGBLegacy, FunctionMetadataEntry::kLegacyRgb},
        {Color::ColorSpace::kSRGB, FunctionMetadataEntry::kColorRgb},
        {Color::ColorSpace::kSRGBLinear, FunctionMetadataEntry::kColorRgb},
        {Color::ColorSpace::kDisplayP3, FunctionMetadataEntry::kColorRgb},
        {Color::ColorSpace::kA98RGB, FunctionMetadataEntry::kColorRgb},
        {Color::ColorSpace::kProPhotoRGB, FunctionMetadataEntry::kColorRgb},
        {Color::ColorSpace::kRec2020, FunctionMetadataEntry::kColorRgb},
        {Color::ColorSpace::kXYZD50, FunctionMetadataEntry::kColorXyz},
        {Color::ColorSpace::kXYZD65, FunctionMetadataEntry::kColorXyz},
        {Color::ColorSpace::kLab, FunctionMetadataEntry::kLab},
        {Color::ColorSpace::kOklab, FunctionMetadataEntry::kOkLab},
        {Color::ColorSpace::kLch, FunctionMetadataEntry::kLch},
        {Color::ColorSpace::kOklch, FunctionMetadataEntry::kOkLch},
        {Color::ColorSpace::kHSL, FunctionMetadataEntry::kHsl},
        {Color::ColorSpace::kHWB, FunctionMetadataEntry::kHwb},
    });

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

// https://www.w3.org/TR/css-color-5/#relative-colors
// e.g. lab(from magenta l a b), consume the "magenta" after the from. The
// result needs to be a blink::Color as we need actual values for the color
// parameters.
bool ConsumeRelativeOriginColor(CSSParserTokenRange& args,
                                const CSSParserContext& context,
                                Color& result) {
  if (!RuntimeEnabledFeatures::CSSRelativeColorEnabled()) {
    return false;
  }
  if (CSSValue* css_color = css_parsing_utils::ConsumeColor(args, context)) {
    if (auto* color_value = DynamicTo<cssvalue::CSSColor>(css_color)) {
      result = color_value->Value();
      return true;
    } else if (auto* css_color_mix_value =
                   DynamicTo<cssvalue::CSSColorMixValue>(css_color)) {
      // TODO(crbug.com/41492196): Support color-mix as origin color.
      return false;
    } else {
      CSSValueID value_id = To<CSSIdentifierValue>(css_color)->GetValueID();
      // TODO(crbug.com/325309578): Just like with
      // css_parsing_utils::ResolveColor(), currentcolor is not currently
      // handled.
      if (value_id == CSSValueID::kCurrentcolor) {
        return false;
      }
      // TODO(crbug.com/40935612): Handle color scheme.
      const ui::ColorProvider* color_provider =
          context.GetDocument()
              ? context.GetDocument()->GetColorProviderForPainting(
                    mojom::blink::ColorScheme::kLight)
              : nullptr;
      result = StyleColor::ColorFromKeyword(
          value_id, mojom::blink::ColorScheme::kLight, color_provider);
      return true;
    }
  }
  return false;
}

std::optional<double> ConsumeRelativeColorChannel(
    CSSParserTokenRange& input_range,
    const CSSParserContext& context,
    const HashMap<CSSValueID, double>& color_channel_keyword_values,
    CalculationResultCategorySet expected_categories,
    const double percentage_base = 0) {
  const CSSParserToken& token = input_range.Peek();
  // Relative color channels can be calc() functions with color channel
  // replacements. e.g. In "color(from magenta srgb calc(r / 2) 0 0)", the
  // "calc" should substitute "1" for "r" (magenta has a full red channel).
  if (token.GetType() == kFunctionToken) {
    using enum CSSMathExpressionNode::Flag;
    using Flags = CSSMathExpressionNode::Flags;

    // Don't consume the range if the parsing fails.
    CSSParserTokenRange calc_range = input_range;
    CSSMathFunctionValue* calc_value = CSSMathFunctionValue::Create(
        CSSMathExpressionNode::ParseMathFunction(
            token.FunctionId(), css_parsing_utils::ConsumeFunction(calc_range),
            context, Flags({AllowPercent}), kCSSAnchorQueryTypesNone,
            color_channel_keyword_values),
        CSSPrimitiveValue::ValueRange::kAll);
    if (calc_value) {
      const CalculationResultCategory category = calc_value->Category();
      if (!expected_categories.Has(category)) {
        return std::nullopt;
      }
      double value;
      switch (category) {
        case kCalcNumber:
          value = calc_value->GetDoubleValueWithoutClamping();
          break;
        case kCalcPercent:
          value = calc_value->GetDoubleValue() / 100;
          value *= percentage_base;
          break;
        case kCalcAngle:
          value = calc_value->ComputeDegrees();
          break;
        default:
          NOTREACHED();
          return std::nullopt;
      }
      // Consume the range, since it has succeeded.
      input_range = calc_range;
      return value;
    }
  }

  // This is for just single variable swaps without calc(). e.g. The "l" in
  // "lab(from cyan l 0.5 0.5)".
  if (color_channel_keyword_values.Contains(token.Id())) {
    input_range.ConsumeIncludingWhitespace();
    return color_channel_keyword_values.at(token.Id());
  }

  return std::nullopt;
}

// https://www.w3.org/TR/css-color-4/#color-function
bool IsValidColorSpaceForColorFunction(Color::ColorSpace color_space) {
  return color_space == Color::ColorSpace::kSRGB ||
         color_space == Color::ColorSpace::kSRGBLinear ||
         color_space == Color::ColorSpace::kDisplayP3 ||
         color_space == Color::ColorSpace::kA98RGB ||
         color_space == Color::ColorSpace::kProPhotoRGB ||
         color_space == Color::ColorSpace::kRec2020 ||
         color_space == Color::ColorSpace::kXYZD50 ||
         color_space == Color::ColorSpace::kXYZD65;
}

}  // namespace

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
    if (!IsValidColorSpaceForColorFunction(color_space_)) {
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

  auto function_entry = kColorSpaceFunctionMap.find(color_space_);
  CHECK_NE(function_entry, kColorSpaceFunctionMap.end());
  auto function_metadata_entry =
      kFunctionMetadataMap.find(function_entry->second);
  CHECK_NE(function_metadata_entry, kFunctionMetadataMap.end());
  function_metadata_ = &function_metadata_entry->second;

  if (is_relative_color_) {
    origin_color_.ConvertToColorSpace(color_space_);
    // Relative color syntax requires "channel keyword" substitutions for color
    // channels. Each color space has three "channel keywords", plus "alpha",
    // that correspond to the three parameters stored on the origin color. This
    // function generates a map between the channel keywords and the stored
    // values in order to make said substitutions. e.g. color(from magenta srgb
    // r g b) will need to generate srgb keyword values for the origin color
    // "magenta". This will produce a map like: {CSSValueID::kR: 1,
    // CSSValueID::kG: 0, CSSValueID::kB: 1, CSSValueID::kAlpha: 1}.
    channel_keyword_values_ = {
        {function_metadata_->channel_name[0], origin_color_.Param0()},
        {function_metadata_->channel_name[1], origin_color_.Param1()},
        {function_metadata_->channel_name[2], origin_color_.Param2()},
        {CSSValueID::kAlpha, origin_color_.Alpha()},
    };
  }

  return true;
}

// ConsumeHue takes an angle as input (as angle in radians or in degrees, or as
// plain number in degrees) and returns a plain number in degrees.
static std::optional<double> ConsumeHue(CSSParserTokenRange& range,
                                        const CSSParserContext& context) {
  CSSPrimitiveValue* value =
      css_parsing_utils::ConsumeAngle(range, context, std::nullopt);
  double angle_value;
  if (!value) {
    value = css_parsing_utils::ConsumeNumber(
        range, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!value) {
      return std::nullopt;
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
  if (css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(args)) {
    channel_types_[i] = ChannelType::kNone;
    has_none_ = true;
    return true;
  }

  if (ColorChannelIsHue(color_space_, i)) {
    if ((channels_[i] = ConsumeHue(args, context))) {
      channel_types_[i] = ChannelType::kNumber;
    } else if (is_relative_color_) {
      if ((channels_[i] = ConsumeRelativeColorChannel(
               args, context, channel_keyword_values_,
               {kCalcNumber, kCalcAngle}))) {
        channel_types_[i] = ChannelType::kRelative;
      }
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

  CSSPrimitiveValue* temp;
  if ((temp = css_parsing_utils::ConsumeNumber(
           args, context, CSSPrimitiveValue::ValueRange::kAll))) {
    channels_[i] = temp->GetDoubleValueWithoutClamping();
    channel_types_[i] = ChannelType::kNumber;
    return true;
  }

  if ((temp = css_parsing_utils::ConsumePercent(
           args, context, CSSPrimitiveValue::ValueRange::kAll))) {
    const double value = temp->GetDoubleValue();
    channels_[i] = (value / 100.0) * function_metadata_->channel_percentage[i];
    channel_types_[i] = ChannelType::kPercentage;
    return true;
  }

  if (is_relative_color_) {
    channel_types_[i] = ChannelType::kRelative;
    if ((channels_[i] = ConsumeRelativeColorChannel(
             args, context, channel_keyword_values_,
             {kCalcNumber, kCalcPercent},
             function_metadata_->channel_percentage[i]))) {
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
           args, context, {{CSSValueID::kAlpha, origin_color_.Alpha()}},
           {kCalcNumber, kCalcPercent}, 1.0))) {
    // Same as above, check if the channel contains only the keyword
    // "alpha", because that can be either an rgb or an xyz param.
    return true;
  }

  if (is_relative_color_ && (alpha_ = ConsumeRelativeColorChannel(
                                 args, context, channel_keyword_values_,
                                 {kCalcNumber, kCalcPercent}, 1.0))) {
    return true;
  }

  return false;
}

bool ColorFunctionParser::MakePerColorSpaceAdjustments() {
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
        if (uses_bare_numbers && is_legacy_syntax_) {
          return false;
        }
        uses_percentage = true;
      } else if (channel_types_[i] == ChannelType::kNumber) {
        if (uses_percentage && is_legacy_syntax_) {
          return false;
        }
        uses_bare_numbers = true;
      }

      if (!isfinite(channels_[i].value())) {
        channels_[i].value() = channels_[i].value() > 0 ? 255.0 : 0;
      } else if (!is_relative_color_) {
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
      } else if (channel_types_[i] == ChannelType::kPercentage) {
        channels_[i] = channels_[i].value() / 100.0;
      }
      if (channels_[i].has_value() && is_legacy_syntax_) {
        channels_[i] = ClampTo<double>(channels_[i].value(), 0.0, 1.0);
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
    // Potentially expect a separator after the first and second channel. The
    // separator for a potential alpha channel is handled below.
    if (i < 2) {
      const bool matched_comma =
          css_parsing_utils::ConsumeCommaIncludingWhitespace(args);
      if (is_legacy_syntax_) {
        // We've parsed one separating comma token, so we expect the second
        // separator to match.
        if (!matched_comma) {
          return false;
        }
      } else if (matched_comma) {
        if (is_relative_color_) {
          return false;
        }
        is_legacy_syntax_ = true;
      }
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

  // "None" is not a part of the legacy syntax.
  if (!args.AtEnd() ||
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
