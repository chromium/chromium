// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color_mix_value.h"

#include <algorithm>
#include <array>

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

void CSSColorMixItem::Trace(Visitor* visitor) const {
  visitor->Trace(color);
  visitor->Trace(percentage);
}

void CSSColorMixValue::NormalizePercentages(
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

  const double percentage_sum = p1 + p2;
  // Use a progress percentage of 0.5 when the combined percentage is zero.
  // https://www.w3.org/TR/css-color-5/#color-mix-result
  // https://github.com/w3c/csswg-drafts/issues/14013
  if (percentage_sum == 0.0) {
    mix_amount = 0.5;
    alpha_multiplier = 0.0;
    return;
  }

  mix_amount = p2 / percentage_sum;
  alpha_multiplier = percentage_sum <= 1.0 ? percentage_sum : 1.0;
}

bool CSSColorMixValue::Equals(const CSSColorMixValue& other) const {
  return items_ == other.items_ &&
         color_interpolation_space_ == other.color_interpolation_space_ &&
         hue_interpolation_method_ == other.hue_interpolation_method_;
}

std::pair<const CSSPrimitiveValue*, const CSSPrimitiveValue*>
CSSColorMixValue::PercentageValuesForSerialization(
    const CSSPrimitiveValue* p1,
    const CSSPrimitiveValue* p2) {
  const auto* p1_literal = DynamicTo<CSSNumericLiteralValue>(p1);
  const auto* p2_literal = DynamicTo<CSSNumericLiteralValue>(p2);

  if (p1_literal) {
    const double p1_percent = p1_literal->ComputePercentage();
    if (p1_percent == 50.0) {
      if (!p2 || (p2_literal && p2_literal->ComputePercentage() == 50.0)) {
        return {nullptr, nullptr};
      }
    } else if (!p2) {
      return {p1, p1->SubtractFrom(100.0,
                                   CSSPrimitiveValue::UnitType::kPercentage)};
    }
  } else if (!p1 && p2_literal) {
    const double p2_percent = p2_literal->ComputePercentage();
    if (p2_percent == 50.0) {
      return {nullptr, nullptr};
    }
    return {p2->SubtractFrom(100.0, CSSPrimitiveValue::UnitType::kPercentage),
            p2};
  }

  return {p1, p2};
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
      PercentageValuesForSerialization(items_[0].percentage,
                                       items_[1].percentage);
  const std::array<const CSSPrimitiveValue*, 2> serialized_percentages = {
      percentage1_value, percentage2_value};

  for (wtf_size_t i = 0; i < items_.size(); ++i) {
    if (i > 0) {
      result.Append(", ");
    }
    result.Append(items_[i].color->CssText());
    if (serialized_percentages[i]) {
      result.Append(' ');
      result.Append(serialized_percentages[i]->CssText());
    }
  }
  result.Append(')');
  return result.ReleaseString();
}

bool CSSColorMixValue::HasRandomFunctions() const {
  return std::ranges::any_of(items_, [](const CSSColorMixItem& item) {
    return item.color->HasRandomFunctions() ||
           (item.percentage && item.percentage->HasRandomFunctions());
  });
}

bool CSSColorMixValue::IsComputationallyIndependent() const {
  return std::ranges::all_of(items_, [](const CSSColorMixItem& item) {
    return item.color->IsComputationallyIndependent() &&
           (!item.percentage ||
            item.percentage->IsComputationallyIndependent());
  });
}

void CSSColorMixValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(items_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
