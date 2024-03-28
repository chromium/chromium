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
          value_id, mojom::blink::ColorScheme::kLight, nullptr);
    }
    return std::nullopt;
  }
  if (auto* color_mix_value = DynamicTo<cssvalue::CSSColorMixValue>(value)) {
    auto color1 = TryResolveAtParseTime(color_mix_value->Color1());
    auto color2 = TryResolveAtParseTime(color_mix_value->Color2());
    if (color1 && color2) {
      return StyleColor::UnresolvedColorMix(
                 color_mix_value, StyleColor(*color1), StyleColor(*color2))
          .Resolve(Color());
    }
    return std::nullopt;
  }
  return std::nullopt;
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
    if (auto absolute_color = TryResolveAtParseTime(*css_color)) {
      result = absolute_color.value();
      return true;
    }
    // TODO(crbug.com/325309578): Just like with
    // css_parsing_utils::ResolveColor(), currentcolor is not currently
    // handled.
    // TODO(crbug.com/41492196): Similarly, color-mix() with non-absolute
    // arguments is not supported as an origin color yet.
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
    CSSParserTokenRange& args,
    CSSValueID function_id,
    const CSSParserContext& context) {
  // [from <color>]?
  if (css_parsing_utils::ConsumeIdent<CSSValueID::kFrom>(args)) {
    if (!ConsumeRelativeOriginColor(args, context, origin_color_)) {
      return false;
    }
    is_relative_color_ = true;
  }

  // Get the color space. This will either be the name of the function, or it
  // will be the first argument of the "color" function.
  if (function_id == CSSValueID::kColor) {
    // <predefined-rgb> | <xyz-space>
    color_space_ = ColorSpaceFromColorSpaceArgument(
        args.ConsumeIncludingWhitespace().Id());
    if (color_space_ == Color::ColorSpace::kNone) {
      return false;
    }
  } else {
    color_space_ = ColorSpaceFromFunctionName(function_id);
  }

  auto function_entry = kColorSpaceFunctionMap.find(color_space_);
  CHECK(function_entry != kColorSpaceFunctionMap.end());
  auto function_metadata_entry =
      kFunctionMetadataMap.find(function_entry->second);
  CHECK(function_metadata_entry != kFunctionMetadataMap.end());
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
    std::array<double, 3> channel_values = {
        origin_color_.Param0(), origin_color_.Param1(), origin_color_.Param2()};

    // Convert from the [0 1] range to the [0 100] range for hsl() and
    // hwb(). This is the inverse of the transform in
    // MakePerColorSpaceAdjustments().
    if (color_space_ == Color::ColorSpace::kHSL ||
        color_space_ == Color::ColorSpace::kHWB) {
      channel_values[1] *= 100;
      channel_values[2] *= 100;
    }

    channel_keyword_values_ = {
        {function_metadata_->channel_name[0], channel_values[0]},
        {function_metadata_->channel_name[1], channel_values[1]},
        {function_metadata_->channel_name[2], channel_values[2]},
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
      }
      // Raw numbers are interpreted as percentages in these color spaces.
      if (channels_[i].has_value()) {
        channels_[i] = channels_[i].value() / 100.0;

        if (is_legacy_syntax_) {
          channels_[i] = ClampTo<double>(channels_[i].value(), 0.0, 1.0);
        }
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

  CSSValueID function_id = range.Peek().FunctionId();
  if (!IsValidColorFunction(function_id)) {
    return false;
  }

  CSSParserTokenRange args = css_parsing_utils::ConsumeFunction(range);
  if (!ConsumeColorSpaceAndOriginColor(args, function_id, context)) {
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
  if (is_legacy_syntax_) {
    if (!Color::IsLegacyColorSpace(color_space_)) {
      return false;
    }
    // , <alpha-value>?
    if (css_parsing_utils::ConsumeCommaIncludingWhitespace(args)) {
      expect_alpha = true;
    }
  } else {
    // / <alpha-value>?
    if (css_parsing_utils::ConsumeSlashIncludingWhitespace(args)) {
      expect_alpha = true;
    }
  }
  if (expect_alpha) {
    if (!ConsumeAlpha(args, context)) {
      return false;
    }
  } else if (is_relative_color_) {
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

  // TODO(b/40949047): Counters for out-of-gamut are disabled because of
  // repeated merge-conflict creating reverts. Remove this parameter.
  const bool kEnableCounters = false;
  if (is_relative_color_) {
    context.Count(WebFeature::kCSSRelativeColor);
  } else {
    switch (color_space_) {
      case Color::ColorSpace::kSRGB:
      case Color::ColorSpace::kSRGBLinear:
      case Color::ColorSpace::kDisplayP3:
      case Color::ColorSpace::kA98RGB:
      case Color::ColorSpace::kProPhotoRGB:
      case Color::ColorSpace::kRec2020:
        if (kEnableCounters) {
          context.Count(WebFeature::kCSSColor_SpaceRGB);
          if (!IsInGamutRec2020(result)) {
            context.Count(WebFeature::kCSSColor_SpaceRGB_outOfRec2020);
          }
        }
        break;
      case Color::ColorSpace::kOklab:
      case Color::ColorSpace::kOklch:
        if (kEnableCounters) {
          context.Count(WebFeature::kCSSColor_SpaceOkLxx);
          if (!IsInGamutRec2020(result)) {
            context.Count(WebFeature::kCSSColor_SpaceOkLxx_outOfRec2020);
          }
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

  return true;
}

}  // namespace blink
