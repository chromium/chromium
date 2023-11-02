// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_filter.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/interpolable_shadow.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {
namespace {
double ClampParameter(double value, FilterOperation::OperationType type) {
  switch (type) {
    case FilterOperation::OperationType::kBrightness:
    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kSaturate:
      return ClampTo<double>(value, 0);

    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kOpacity:
    case FilterOperation::OperationType::kSepia:
      return ClampTo<double>(value, 0, 1);

    case FilterOperation::OperationType::kHueRotate:
      return value;

    default:
      NOTREACHED();
      return 0;
  }
}
}  // namespace

// static
std::unique_ptr<InterpolableFilter> InterpolableFilter::MaybeCreate(
    const FilterOperation& filter,
    double zoom) {
  std::unique_ptr<InterpolableValue> value;
  FilterOperation::OperationType type = filter.GetType();
  switch (type) {
    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kHueRotate:
    case FilterOperation::OperationType::kSaturate:
    case FilterOperation::OperationType::kSepia:
      value = std::make_unique<InterpolableNumber>(
          To<BasicColorMatrixFilterOperation>(filter).Amount());
      break;

    case FilterOperation::OperationType::kBrightness:
    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kOpacity:
      value = std::make_unique<InterpolableNumber>(
          To<BasicComponentTransferFilterOperation>(filter).Amount());
      break;

    case FilterOperation::OperationType::kBlur:
      value = InterpolableLength::MaybeConvertLength(
          To<BlurFilterOperation>(filter).StdDeviation(), zoom);
      break;

    case FilterOperation::OperationType::kDropShadow:
      value = InterpolableShadow::Create(
          To<DropShadowFilterOperation>(filter).Shadow(), zoom);
      break;

    case FilterOperation::OperationType::kReference:
      return nullptr;

    default:
      NOTREACHED();
      return nullptr;
  }

  if (!value)
    return nullptr;
  return std::make_unique<InterpolableFilter>(std::move(value), type);
}

// static
std::unique_ptr<InterpolableFilter> InterpolableFilter::MaybeConvertCSSValue(
    const CSSValue& css_value) {
  if (css_value.IsURIValue())
    return nullptr;

  const auto& filter = To<CSSFunctionValue>(css_value);
  DCHECK_LE(filter.length(), 1u);

  std::unique_ptr<InterpolableValue> value;
  FilterOperation::OperationType type =
      FilterOperationResolver::FilterOperationForType(filter.FunctionType());
  switch (type) {
    case FilterOperation::OperationType::kBrightness:
    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kOpacity:
    case FilterOperation::OperationType::kSaturate:
    case FilterOperation::OperationType::kSepia:
    case FilterOperation::OperationType::kHueRotate:
      value = std::make_unique<InterpolableNumber>(
          FilterOperationResolver::ResolveNumericArgumentForFunction(filter));
      break;

    case FilterOperation::OperationType::kBlur:
      value = filter.length() > 0
                  ? InterpolableLength::MaybeConvertCSSValue(filter.Item(0))
                  : InterpolableLength::CreateNeutral();
      break;

    case FilterOperation::OperationType::kDropShadow:
      value = InterpolableShadow::MaybeConvertCSSValue(filter.Item(0));
      break;

    default:
      NOTREACHED();
      return nullptr;
  }

  if (!value)
    return nullptr;
  return std::make_unique<InterpolableFilter>(std::move(value), type);
}

// static
std::unique_ptr<InterpolableFilter> InterpolableFilter::CreateInitialValue(
    FilterOperation::OperationType type) {
  // See https://drafts.fxtf.org/filter-effects-1/#filter-functions for the
  // mapping of OperationType to initial value.
  std::unique_ptr<InterpolableValue> value;
  switch (type) {
    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kSepia:
    case FilterOperation::OperationType::kHueRotate:
      value = std::make_unique<InterpolableNumber>(0);
      break;

    case FilterOperation::OperationType::kBrightness:
    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kOpacity:
    case FilterOperation::OperationType::kSaturate:
      value = std::make_unique<InterpolableNumber>(1);
      break;

    case FilterOperation::OperationType::kBlur:
      value = InterpolableLength::CreateNeutral();
      break;

    case FilterOperation::OperationType::kDropShadow:
      value = InterpolableShadow::CreateNeutral();
      break;

    default:
      NOTREACHED();
      return nullptr;
  }

  return std::make_unique<InterpolableFilter>(std::move(value), type);
}

FilterOperation* InterpolableFilter::CreateFilterOperation(
    const StyleResolverState& state) const {
  switch (type_) {
    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kHueRotate:
    case FilterOperation::OperationType::kSaturate:
    case FilterOperation::OperationType::kSepia: {
      double value =
          ClampParameter(To<InterpolableNumber>(*value_).Value(), type_);
      return MakeGarbageCollected<BasicColorMatrixFilterOperation>(value,
                                                                   type_);
    }

    case FilterOperation::OperationType::kBrightness:
    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kOpacity: {
      double value =
          ClampParameter(To<InterpolableNumber>(*value_).Value(), type_);
      return MakeGarbageCollected<BasicComponentTransferFilterOperation>(value,
                                                                         type_);
    }

    case FilterOperation::OperationType::kBlur: {
      Length std_deviation = To<InterpolableLength>(*value_).CreateLength(
          state.CssToLengthConversionData(), Length::ValueRange::kNonNegative);
      return MakeGarbageCollected<BlurFilterOperation>(std_deviation);
    }

    case FilterOperation::OperationType::kDropShadow: {
      ShadowData shadow_data =
          To<InterpolableShadow>(*value_).CreateShadowData(state);
      if (shadow_data.GetColor().IsCurrentColor())
        shadow_data.OverrideColor(Color::kBlack);
      return MakeGarbageCollected<DropShadowFilterOperation>(shadow_data);
    }

    default:
      NOTREACHED();
      return nullptr;
  }
}

void InterpolableFilter::Add(const InterpolableValue& other) {
  value_->Add(*To<InterpolableFilter>(other).value_);
  // The following types have an initial value of 1, so addition for them is
  // one-based: result = value_ + other.value_ - 1
  switch (type_) {
    case FilterOperation::OperationType::kBrightness:
    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kOpacity:
    case FilterOperation::OperationType::kSaturate:
    case FilterOperation::OperationType::kSepia:
      value_->Add(*std::make_unique<InterpolableNumber>(-1));
      break;
    default:
      break;
  }
}

void InterpolableFilter::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableFilter& other_filter = To<InterpolableFilter>(other);
  value_->AssertCanInterpolateWith(*other_filter.value_);
  DCHECK_EQ(type_, other_filter.type_);
}

void InterpolableFilter::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const InterpolableFilter& filter_to = To<InterpolableFilter>(to);
  InterpolableFilter& filter_result = To<InterpolableFilter>(result);
  value_->Interpolate(*filter_to.value_, progress, *filter_result.value_);
}

}  // namespace blink
