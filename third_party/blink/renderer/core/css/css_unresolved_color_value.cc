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

// Same clamping as in ColorFunctionParser::MakePerColorSpaceAdjustments().
static double ClampChannel(double val) {
  if (!isfinite(val)) {
    return val > 0 ? 255.0 : 0;
  } else {
    return ClampTo<double>(val, 0.0, 255.0);
  }
}

static void AppendChannel(const CSSPrimitiveValue* value,
                          Color::ColorSpace color_space,
                          StringBuilder& result) {
  if (value) {
    if (const auto* literal = DynamicTo<CSSNumericLiteralValue>(value)) {
      double val = literal->ClampedDoubleValue();
      if (value->IsPercentage()) {
        val /= 100.0;
      }
      result.AppendNumber(ClampChannel(val));
    } else {
      // https://drafts.csswg.org/css-color-4/#resolving-sRGB-values
      // “For historical reasons, when calc() in sRGB colors resolves
      // to a single value, the declared value serialises without the
      // "calc(" ")" wrapper.”
      const CSSMathFunctionValue* calc = DynamicTo<CSSMathFunctionValue>(value);
      if (calc && calc->ExpressionNode()->IsNumericLiteral() &&
          color_space == Color::ColorSpace::kSRGBLegacy) {
        result.AppendNumber(
            ClampChannel(calc->ExpressionNode()->DoubleValue()));
      } else {
        result.Append(value->CssText());
      }
    }
  } else {
    result.Append("none");
  }
}

WTF::String CSSUnresolvedColorValue::CustomCSSText() const {
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
  AppendChannel(channels_[0], color_space_, result);
  result.Append(" ");
  AppendChannel(channels_[1], color_space_, result);
  result.Append(" ");
  AppendChannel(channels_[2], color_space_, result);
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
