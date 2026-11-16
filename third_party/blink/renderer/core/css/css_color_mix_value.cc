// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color_mix_value.h"

#include <algorithm>
#include <functional>
#include <optional>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

namespace {

struct SpecifiedPercentages {
  explicit SpecifiedPercentages(
      base::span<const std::optional<double>> percentages) {
    for (const std::optional<double>& percentage : percentages) {
      if (percentage) {
        specified_sum += *percentage;
      } else {
        ++omitted_count;
      }
    }
  }

  // https://drafts.csswg.org/css-values-5/#normalize-mix-percentages
  double OmittedShare(double target_sum) const {
    return omitted_count
               ? std::max(0.0, target_sum - specified_sum) / omitted_count
               : 0.0;
  }

  double specified_sum = 0.0;
  wtf_size_t omitted_count = 0;
};

// Numeric functions such as calc() are not range-checked when parsing, so they
// are clamped here.
// https://drafts.csswg.org/css-values-4/#functional-numeric
std::optional<double> ResolvePercentage(
    const CSSPrimitiveValue* percentage,
    const CSSLengthResolver& length_resolver) {
  if (!percentage) {
    return std::nullopt;
  }
  return ClampTo<double>(percentage->ComputePercentage(length_resolver), 0.0,
                         100.0) /
         100.0;
}

}  // namespace

void CSSColorMixItem::Trace(Visitor* visitor) const {
  visitor->Trace(color);
  visitor->Trace(percentage);
}

// static
CSSColorMixValue::NormalizedPercentages
CSSColorMixValue::NormalizeMixPercentages(
    base::span<const std::optional<double>> percentages) {
  const SpecifiedPercentages specified(percentages);
  const double omitted_percentage = specified.OmittedShare(1.0);
  // Omitted percentages fill any gap up to 1. Adding them up instead could
  // round to just below 1, and the alpha multiplier would then not be exactly
  // 1.
  const double total = specified.omitted_count
                           ? std::max(specified.specified_sum, 1.0)
                           : specified.specified_sum;
  // This deviates from the spec. For color-mix(), the spec also scales the
  // percentages up when they add up to less than 1 (the "force normalization"
  // flag). We don't, because UnresolvedColorMix::ToCSSValue() serializes the
  // percentages, and they are the only way to express the alpha multiplier in
  // color-mix(). Mixing only depends on the ratios between the percentages, so
  // the color is the same.
  // https://drafts.csswg.org/css-color-5/#color-mix-result
  const double divisor = std::max(total, 1.0);

  NormalizedPercentages result;
  result.percentages = Vector<double, 2>(
      percentages, [&](const std::optional<double>& percentage) {
        return percentage.value_or(omitted_percentage) / divisor;
      });
  result.alpha_multiplier = std::min(total, 1.0);
  return result;
}

CSSColorMixValue::NormalizedPercentages
CSSColorMixValue::NormalizeMixPercentages(
    const CSSLengthResolver& length_resolver) const {
  return NormalizeMixPercentages(Vector<std::optional<double>, 2>(
      items_, [&length_resolver](const CSSColorMixItem& item) {
        return ResolvePercentage(item.percentage, length_resolver);
      }));
}

void CSSColorMixValue::NormalizePercentages(
    const CSSPrimitiveValue* percentage1,
    const CSSPrimitiveValue* percentage2,
    double& mix_amount,
    double& alpha_multiplier,
    const CSSLengthResolver& length_resolver) {
  const std::optional<double> percentages[] = {
      ResolvePercentage(percentage1, length_resolver),
      ResolvePercentage(percentage2, length_resolver)};
  const NormalizedPercentages normalized = NormalizeMixPercentages(percentages);
  const double sum = normalized.percentages[0] + normalized.percentages[1];
  // Use a progress percentage of 0.5 when the combined percentage is zero.
  // https://www.w3.org/TR/css-color-5/#color-mix-result
  // https://github.com/w3c/csswg-drafts/issues/14013
  mix_amount = sum == 0.0 ? 0.5 : normalized.percentages[1] / sum;
  alpha_multiplier = normalized.alpha_multiplier;
}

bool CSSColorMixValue::Equals(const CSSColorMixValue& other) const {
  return items_ == other.items_ &&
         color_interpolation_space_ == other.color_interpolation_space_ &&
         hue_interpolation_method_ == other.hue_interpolation_method_;
}

// static
CSSColorMixValue::PercentageVector
CSSColorMixValue::PercentageValuesForSerialization(
    base::span<const Member<const CSSPrimitiveValue>> percentages) {
  Vector<std::optional<double>, 2> values;
  for (const auto& percentage : percentages) {
    if (!percentage) {
      values.push_back(std::nullopt);
    } else if (const auto* literal =
                   DynamicTo<CSSNumericLiteralValue>(percentage.Get())) {
      values.push_back(literal->ComputePercentage());
    } else {
      // Omitted percentages can't be computed when a specified one is not a
      // literal (e.g. calc()), so serialize the percentages as specified.
      return PercentageVector(percentages, std::identity());
    }
  }
  const double omitted_percentage =
      SpecifiedPercentages(values).OmittedShare(100.0);

  // Percentages are omitted only when they are all equal to 100% / N. This
  // deviates from the spec, which also omits equal percentages above that
  // (e.g. "red 75%, blue 75%"), because color-valid-color-mix-function.html
  // expects them to be kept, as other engines do.
  // https://github.com/w3c/csswg-drafts/issues/14465
  const double equal_share = 100.0 / percentages.size();
  if (std::ranges::all_of(values, [&](const std::optional<double>& value) {
        return value.value_or(omitted_percentage) == equal_share;
      })) {
    return PercentageVector(base::checked_cast<wtf_size_t>(percentages.size()));
  }

  const CSSPrimitiveValue* omitted_value = CSSNumericLiteralValue::Create(
      omitted_percentage, CSSPrimitiveValue::UnitType::kPercentage);
  return PercentageVector(
      percentages,
      [omitted_value](const Member<const CSSPrimitiveValue>& percentage)
          -> const CSSPrimitiveValue* {
        return percentage ? percentage.Get() : omitted_value;
      });
}

// static
std::pair<const CSSPrimitiveValue*, const CSSPrimitiveValue*>
CSSColorMixValue::PercentageValuesForSerialization(
    const CSSPrimitiveValue* p1,
    const CSSPrimitiveValue* p2) {
  const PercentageVector result =
      PercentageValuesForSerialization(PercentageVector{p1, p2});
  return {result[0].Get(), result[1].Get()};
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

  const PercentageVector serialized_percentages =
      PercentageValuesForSerialization(
          PercentageVector(items_, &CSSColorMixItem::percentage));

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
