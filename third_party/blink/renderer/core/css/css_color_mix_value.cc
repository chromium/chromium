// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color_mix_value.h"

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

bool CSSColorMixValue::NormalizePercentages(
    const CSSPrimitiveValue* percentage1,
    const CSSPrimitiveValue* percentage2,
    double& mix_amount,
    double& alpha_multiplier,
    const CSSLengthResolver& length_resolver) {
  double p1 = 0.5;
  if (percentage1) {
    p1 = ClampTo<double>(percentage1->ComputePercentage(length_resolver), 0.0,
                         100.0) /
         100.0;
  }
  double p2 = 0.5;
  if (percentage2) {
    p2 = ClampTo<double>(percentage2->ComputePercentage(length_resolver), 0.0,
                         100.0) /
         100.0;
  }

  if (percentage1 && !percentage2) {
    p2 = 1.0 - p1;
  } else if (percentage2 && !percentage1) {
    p1 = 1.0 - p2;
  }

  if (p1 == 0.0 && p2 == 0.0) {
    return false;
  }

  alpha_multiplier = 1.0;

  double scale = p1 + p2;
  if (scale != 0.0) {
    p1 /= scale;
    p2 /= scale;
    if (scale <= 1.0) {
      alpha_multiplier = scale;
    }
  }

  mix_amount = p2;
  if (p1 == 0.0) {
    mix_amount = 1.0;
  }

  return true;
}

Color CSSColorMixValue::Mix(const Color& color1,
                            const Color& color2,
                            const CSSLengthResolver& length_resolver) const {
  double alpha_multiplier;
  double mix_amount;
  if (!NormalizePercentages(mix_amount, alpha_multiplier, length_resolver)) {
    return Color();
  }
  return Color::FromColorMix(ColorInterpolationSpace(),
                             HueInterpolationMethod(), color1, color2,
                             mix_amount, alpha_multiplier);
}

bool CSSColorMixValue::Equals(const CSSColorMixValue& other) const {
  return color1_ == other.color1_ && color2_ == other.color2_ &&
         percentage1_ == other.percentage1_ &&
         percentage2_ == other.percentage2_ &&
         color_interpolation_space_ == other.color_interpolation_space_ &&
         hue_interpolation_method_ == other.hue_interpolation_method_;
}

std::pair<const CSSPrimitiveValue*, const CSSPrimitiveValue*>
CSSColorMixValue::PercentageValuesForSerialization(
    const CSSPrimitiveValue* p1,
    const CSSPrimitiveValue* p2) {
  if (p1) {
    if (auto* p1_literal = DynamicTo<CSSNumericLiteralValue>(*p1)) {
      const double p1_literal_percent = p1_literal->ComputePercentage();
      if (p2) {
        if (auto* p2_literal = DynamicTo<CSSNumericLiteralValue>(*p2)) {
          const double p2_literal_percent = p2_literal->ComputePercentage();
          if (p1_literal_percent == 50.0 && p2_literal_percent == 50.0) {
            return {nullptr, nullptr};
          }
          if (p1_literal_percent + p2_literal_percent == 100.0) {
            return {p1, nullptr};
          }
        }
      } else {
        if (p1_literal_percent == 50.0) {
          return {nullptr, nullptr};
        }
      }
    }
    return {p1, p2};
  }
  if (p2) {
    if (auto* p2_literal = DynamicTo<CSSNumericLiteralValue>(*p2)) {
      if (p2_literal->ComputePercentage() == 50.0) {
        return {nullptr, nullptr};
      }
      return {p2->SubtractFrom(100.0, CSSPrimitiveValue::UnitType::kPercentage),
              nullptr};
    }
    return {nullptr, p2};
  }
  return {nullptr, nullptr};
}

String CSSColorMixValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("color-mix(");

  // Per CSS Color 5, the default interpolation space is oklab with shorter hue.
  // Default values are omitted from serialization.
  // https://drafts.csswg.org/css-color-5/#color-mix-space
  const bool is_default_interpolation =
      color_interpolation_space_ == Color::ColorSpace::kOklab &&
      hue_interpolation_method_ == Color::HueInterpolationMethod::kShorter;

  if (!is_default_interpolation) {
    result.Append("in ");
    result.Append(Color::SerializeInterpolationSpace(
        color_interpolation_space_, hue_interpolation_method_));
    result.Append(", ");
  }

  auto [percentage1_value, percentage2_value] =
      PercentageValuesForSerialization(percentage1_, percentage2_);

  result.Append(color1_->CssText());
  if (percentage1_value) {
    result.Append(' ');
    result.Append(percentage1_value->CssText());
  }
  result.Append(", ");
  result.Append(color2_->CssText());
  if (percentage2_value) {
    result.Append(' ');
    result.Append(percentage2_value->CssText());
  }
  result.Append(')');
  return result.ReleaseString();
}

void CSSColorMixValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(color1_);
  visitor->Trace(color2_);
  visitor->Trace(percentage1_);
  visitor->Trace(percentage2_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
