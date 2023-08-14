// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_color_function_parser.h"
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

bool ColorFunctionParser::ConsumeColorSpace(CSSParserTokenRange& range,
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
    color_space_ =
        CSSValueIDToColorSpace(args.ConsumeIncludingWhitespace().Id());
    if (!Color::IsPredefinedColorSpace(color_space_)) {
      return false;
    }
  }

  return true;
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
    is_legacy_syntax_ = true;
  }
  if (css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(args)) {
    channel_types_[i] = ChannelType::kNone;
    has_none_ = true;
    return true;
  }

  if (ColorChannelIsHue(color_space_, i)) {
    temp = css_parsing_utils::ConsumeHue(args, context, absl::nullopt);
    if (temp) {
      channels_[i] = temp->GetDoubleValue();
      channel_types_[i] = ChannelType::kNumber;
      return true;
    } else {
      return false;
    }
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

  // Missing components should not parse.
  return false;
}

bool ColorFunctionParser::ConsumeAlpha(CSSParserTokenRange& args,
                                       const CSSParserContext& context) {
  CSSPrimitiveValue* temp;
  if ((temp = css_parsing_utils::ConsumeNumber(
           args, context, CSSPrimitiveValue::ValueRange::kAll))) {
    alpha_ = temp->GetDoubleValueWithoutClamping();
    if (isfinite(alpha_.value())) {
      alpha_ = ClampTo<double>(alpha_.value(), 0.0, 1.0);
    }
    return true;
  }

  if ((temp = css_parsing_utils::ConsumePercent(
           args, context, CSSPrimitiveValue::ValueRange::kAll))) {
    alpha_ = temp->GetDoubleValue() / 100.0;
    return true;
  }

  if (css_parsing_utils::ConsumeIdent<CSSValueID::kNone>(args)) {
    has_none_ = true;
    alpha_.reset();
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
      if (channel_types_[i] == ChannelType::kPercentage) {
        if (uses_bare_numbers) {
          return false;
        }
        uses_percentage = true;
      } else if (channel_types_[i] == ChannelType::kNumber) {
        if (uses_percentage) {
          return false;
        }
        uses_bare_numbers = true;
        channels_[i].value() /= 255.0;
      }
    }
    // TODO(crbug.com/1399566): There are many code paths that still compress
    // alpha to be an 8-bit integer. If it is not explicitly compressed here,
    // tests will fail due to some paths doing this compression and others not.
    // See compositing/background-color/background-color-alpha.html for example.
    // Ideally we would allow alpha to be any float value, but we have to clean
    // up all spots where this compression happens before this is possible.
    if (alpha_.has_value() && isfinite(alpha_.value())) {
      alpha_ = round(alpha_.value() * 255.0) / 255.0;
    }
  }

  if (color_space_ == Color::ColorSpace::kHWB) {
    // Legacy syntax is not allowed for hwb().
    if (is_legacy_syntax_) {
      return false;
    }
    // w and b must be percentages or relative color channels.
    if (channel_types_[1] == ChannelType::kNumber ||
        channel_types_[2] == ChannelType::kNumber) {
      return false;
    }
  }

  if (color_space_ == Color::ColorSpace::kHSL) {
    // 2nd and 3rd parameters of hsl() must be percentages or "none" and clamped
    // to the range [0, 1].
    for (int i : {1, 2}) {
      if (channel_types_[i] == ChannelType::kNumber) {
        return false;
      }
      if (channel_types_[i] == ChannelType::kPercentage) {
        channels_[i] = ClampTo<double>(channels_[i].value(), 0.0, 1.0);
      }
    }
  }

  // For historical reasons, the "hue" of hwb() and hsl() are stored in the
  // range [0, 6].
  if ((color_space_ == Color::ColorSpace::kHSL ||
       color_space_ == Color::ColorSpace::kHWB) &&
      channel_types_[0] == ChannelType::kNumber) {
    channels_[0].value() /= 60.0;
  }

  // Lightness is stored in the range [0, 100] for lab(), oklab(), lch() and
  // oklch(). For oklab() and oklch() input for lightness is in the range [0,
  // 1].
  if (Color::IsLightnessFirstComponent(color_space_)) {
    if (channel_types_[0] == ChannelType::kPercentage) {
      channels_[0].value() *= 100.0;
    } else if ((color_space_ == Color::ColorSpace::kOklab ||
                color_space_ == Color::ColorSpace::kOklch) &&
               channel_types_[0] == ChannelType::kNumber) {
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

  if (!ConsumeColorSpace(range, context, args)) {
    return false;
  }

  for (int i = 0; i < 3; i++) {
    if (!ConsumeChannel(args, context, i)) {
      return false;
    }
  }

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

  // "None" is not a part of the legacy syntax.
  if (!args.AtEnd() || (is_legacy_syntax_ && has_none_)) {
    return false;
  }

  if (!MakePerColorSpaceAdjustments()) {
    return false;
  }

  result = Color::FromColorSpace(color_space_, channels_[0], channels_[1],
                                 channels_[2], alpha_);
  // The parsing was successful, so we need to consume the input.
  input_range = range;
  return true;
}

}  // namespace blink
