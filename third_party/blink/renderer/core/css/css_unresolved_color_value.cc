// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_unresolved_color_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/color_function.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink::cssvalue {

// Percentage-to-absolute conversion factors
static constexpr double kLabABBase = 125.0;
static constexpr double kOklabABBase = 0.4;
static constexpr double kLchChromaBase = 150.0;
static constexpr double kOklchChromaBase = 0.4;
static constexpr double kRGBBase = 255.0;

// Maximum values for clamping
static constexpr double kLabLightnessMax = 100.0;
static constexpr double kOklabLightnessMax = 1.0;
static constexpr double kLchLightnessMax = 100.0;
static constexpr double kOklchLightnessMax = 1.0;
static constexpr double kRGBChannelMax = 255.0;

// Apply color-space-specific clamping and percentage conversion
static double ProcessChannelValue(double val,
                                  bool is_percentage,
                                  Color::ColorSpace color_space,
                                  int channel_index) {
  switch (color_space) {
    case Color::ColorSpace::kHSL:
      // HSL: [0]=Hue, [1]=Saturation, [2]=Lightness
      // Saturation can not be negative.
      if (channel_index == 1) {
        val = std::max(0.0, val);
      }
      return val;

    case Color::ColorSpace::kLab:
      // Lab: [0]=Lightness, [1]=a, [2]=b
      // Clamp lightness to its expected range.
      if (channel_index == 0) {
        val = ClampTo<double>(val, 0.0, kLabLightnessMax);
      }
      // L% stays as-is, a/b% need conversion (100% = 125).
      if (is_percentage) {
        if (channel_index == 1 || channel_index == 2) {
          return val * kLabABBase / 100.0;
        }
      }
      return val;

    case Color::ColorSpace::kOklab:
      // Oklab: [0]=Lightness, [1]=a, [2]=b
      // Clamp lightness to its expected range.
      if (channel_index == 0) {
        val = ClampTo<double>(val, 0.0, kOklabLightnessMax);
      }
      // L% needs scaling (100% = 1), a/b% need conversion (100% = 0.4).
      if (is_percentage) {
        if (channel_index == 0) {
          return val / 100.0;
        } else {
          return val * kOklabABBase / 100.0;
        }
      }
      return val;

    case Color::ColorSpace::kLch:
      // Lch: [0]=Lightness, [1]=Chroma, [2]=Hue
      // Keep lightness and chroma within their expected ranges.
      if (channel_index == 0) {
        val = ClampTo<double>(val, 0.0, kLchLightnessMax);
      } else if (channel_index == 1) {
        val = std::max(0.0, val);
      }
      // L% stays as-is, C% needs conversion (100% = 150).
      if (is_percentage) {
        if (channel_index == 1) {
          return val * kLchChromaBase / 100.0;
        }
      }
      return val;

    case Color::ColorSpace::kOklch:
      // Oklch: [0]=Lightness, [1]=Chroma, [2]=Hue
      // Keep lightness and chroma within their expected ranges.
      if (channel_index == 0) {
        val = ClampTo<double>(val, 0.0, kOklchLightnessMax);
      } else if (channel_index == 1) {
        val = std::max(0.0, val);
      }
      // L% needs scaling (100% = 1), C% needs conversion (100% = 0.4)
      if (is_percentage) {
        if (channel_index == 0) {
          return val / 100.0;
        } else if (channel_index == 1) {
          return val * kOklchChromaBase / 100.0;
        }
      }
      return val;

    case Color::ColorSpace::kSRGB:
    case Color::ColorSpace::kSRGBLegacy:
      // SRGB: [0]=Red, [1]=Green, [2]=Blue
      // Convert percentages to 0-255 range (100% = 255).
      if (is_percentage) {
        val = val * kRGBBase / 100.0;
      }
      // Clamp to valid RGB range
      return ClampTo<double>(val, 0.0, kRGBChannelMax);

    default:
      // Other color spaces can pass through unchanged.
      return val;
  }
}

static void AppendChannel(const CSSPrimitiveValue* value,
                          Color::ColorSpace color_space,
                          int channel_index,
                          StringBuilder& result) {
  if (!value) {
    result.Append("none");
    return;
  }

  // Numeric literals require specific processing depending on the color space.
  if (const auto* literal = DynamicTo<CSSNumericLiteralValue>(value)) {
    double val =
        ProcessChannelValue(literal->DoubleValue(), value->IsPercentage(),
                            color_space, channel_index);
    result.AppendNumber(val);
    return;
  }

  const CSSMathFunctionValue* calc = DynamicTo<CSSMathFunctionValue>(value);
  // https://drafts.csswg.org/css-color-4/#resolving-sRGB-values
  // “For historical reasons, when calc() in sRGB colors resolves
  // to a single value, the declared value serialises without the
  // "calc(" ")" wrapper.”
  if (calc && calc->ExpressionNode()->IsNumericLiteral() &&
      color_space == Color::ColorSpace::kSRGBLegacy) {
    double val = calc->ExpressionNode()->DoubleValue();
    val = ClampTo<double>(val, 0.0, kRGBChannelMax);
    result.AppendNumber(val);
    return;
  }

  result.Append(value->CssText());
}

String CSSUnresolvedColorValue::CustomCSSText() const {
  // https://drafts.csswg.org/css-color-4/#serializing-color-values
  //
  // TODO: We do not support the full standard here _at all_.
  // We would need to basically unify all of this logic with the one
  // from Color::SerializeAsCSSColor(), except that unresolvable calc()
  // (that does not hit the sRGB single-value exception) would need
  // to be printed as such. Thankfully, this path should be hit
  // rarely enough that it's not the most important issue.
  StringBuilder result;
  const bool serialize_as_color_function =
      Color::IsPredefinedColorSpace(color_space_);
  if (serialize_as_color_function) {
    result.Append("color");
  } else {
    result.Append(Color::ColorSpaceToString(color_space_));
  }
  result.Append("(");
  if (serialize_as_color_function) {
    result.Append(Color::ColorSpaceToString(color_space_));
    result.Append(" ");
  }
  AppendChannel(channels_[0], color_space_, 0, result);
  result.Append(" ");
  AppendChannel(channels_[1], color_space_, 1, result);
  result.Append(" ");
  AppendChannel(channels_[2], color_space_, 2, result);
  if (alpha_) {
    // See if we can find a static value for alpha, so that we can see if it's
    // known to be 1.0 or above. (Note: alpha_->IsOne() would return false for
    // e.g. 1.5.).
    std::optional<double> alpha;
    if (const auto* literal_alpha =
            DynamicTo<CSSNumericLiteralValue>(*alpha_)) {
      alpha = literal_alpha->ClampedDoubleValue();
    } else {
      // See corresponding code in AppendChannel().
      const CSSMathFunctionValue* calc =
          DynamicTo<CSSMathFunctionValue>(alpha_.Get());
      if (calc && calc->ExpressionNode()->IsNumericLiteral() &&
          color_space_ == Color::ColorSpace::kSRGBLegacy) {
        alpha = calc->ExpressionNode()->DoubleValue();
      }
    }
    if (alpha.has_value() && alpha_->IsPercentage()) {
      *alpha /= 100.0;
    }

    if (!alpha.has_value() || !std::isfinite(*alpha) || *alpha < 1.0) {
      result.Append(" / ");
      if (alpha.has_value()) {
        result.AppendNumber(ClampTo<double>(*alpha, 0.0, 1.0));
      } else {
        result.Append(alpha_->CssText());
      }
    }
  } else {
    result.Append(" / none");
  }
  result.Append(")");
  return result.ReleaseString();
}

bool CSSUnresolvedColorValue::Equals(
    const CSSUnresolvedColorValue& other) const {
  return (color_space_ == other.color_space_) &&
         channel_types_ == other.channel_types_ &&
         alpha_channel_type_ == other.alpha_channel_type_ &&
         base::ValuesEquivalent(channels_[0], other.channels_[0]) &&
         base::ValuesEquivalent(channels_[1], other.channels_[1]) &&
         base::ValuesEquivalent(channels_[2], other.channels_[2]) &&
         base::ValuesEquivalent(alpha_, other.alpha_);
}

// Similar to ColorFunctionParser::ResolveColorChannel(),
// but resolves values using CSSLengthResolver, since it is available
// to us (and was not during parsing). Furthermore, it does not deal with
// relative colors.
static double ResolveColorChannel(const CSSPrimitiveValue& value,
                                  ColorFunctionParser::ChannelType channel_type,
                                  double percentage_base,
                                  const CSSLengthResolver& length_resolver) {
  switch (channel_type) {
    case ColorFunctionParser::ChannelType::kNumber:
      if (value.IsAngle()) {
        return value.ComputeDegrees(length_resolver);
      } else {
        return value.ComputeNumber(length_resolver);
      }
    case ColorFunctionParser::ChannelType::kPercentage:
      // NOTE: ComputeNumber() will divide by 100.0 for us.
      return value.ComputeNumber(length_resolver) * percentage_base;
    default:
      NOTREACHED();
  }
}

// See ColorFunctionParser::ConsumeFunctionalSyntaxColor().
Color CSSUnresolvedColorValue::Resolve(
    const CSSLengthResolver& length_resolver) const {
  const ColorFunction::Metadata& function_metadata =
      ColorFunction::MetadataForColorSpace(color_space_);

  // Resolve channel values.
  std::array<std::optional<double>, 3> channels;
  for (int i = 0; i < 3; i++) {
    if (channels_[i]) {
      channels[i] = ResolveColorChannel(*channels_[i], channel_types_[i],
                                        function_metadata.channel_percentage[i],
                                        length_resolver);
    }
  }

  std::optional<double> alpha;
  if (alpha_ &&
      alpha_channel_type_ != ColorFunctionParser::ChannelType::kNone) {
    alpha = ClampTo<double>(
        ResolveColorChannel(*alpha_, alpha_channel_type_,
                            /*percentage_base=*/1.0, length_resolver),
        0.0, 1.0);
  }

  ColorFunctionParser::MakePerColorSpaceAdjustments(
      /*is_relative_color=*/false, /*is_legacy_syntax=*/false, color_space_,
      channels, alpha);

  return Color::FromColorSpace(color_space_, channels[0], channels[1],
                               channels[2], alpha);
}

}  // namespace blink::cssvalue
