// Copyright 2019 The Chromium Authors. All rights reserved.
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
    case FilterOperation::kBrightness:
    case FilterOperation::kContrast:
    case FilterOperation::kSaturate:
      return ClampTo<double>(value, 0);

    case FilterOperation::kGrayscale:
    case FilterOperation::kInvert:
    case FilterOperation::kOpacity:
    case FilterOperation::kSepia:
      return ClampTo<double>(value, 0, 1);

    case FilterOperation::kHueRotate:
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
    case FilterOperation::kGrayscale:
    case FilterOperation::kHueRotate:
    case FilterOperation::kSaturate:
    case FilterOperation::kSepia:
      value = std::make_unique<InterpolableNumber>(
          To<BasicColorMatrixFilterOperation>(filter).Amount());
      break;

    case FilterOperation::kBrightness:
    case FilterOperation::kContrast:
    case FilterOperation::kInvert:
    case FilterOperation::kOpacity:
      value = std::make_unique<InterpolableNumber>(
          To<BasicComponentTransferFilterOperation>(filter).Amount());
      break;

    case FilterOperation::kBlur:
      value = InterpolableLength::MaybeConvertLength(
          To<BlurFilterOperation>(filter).StdDeviation(), zoom);
      break;

    case FilterOperation::kDropShadow:
      value = InterpolableShadow::Create(
          To<DropShadowFilterOperation>(filter).Shadow(), zoom);
      break;

    case FilterOperation::kReference:
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
    case FilterOperation::kBrightness:
    case FilterOperation::kContrast:
    case FilterOperation::kGrayscale:
    case FilterOperation::kInvert:
    case FilterOperation::kOpacity:
    case FilterOperation::kSaturate:
    case FilterOperation::kSepia:
    case FilterOperation::kHueRotate:
      value = std::make_unique<InterpolableNumber>(
          FilterOperationResolver::ResolveNumericArgumentForFunction(filter));
      break;

    case FilterOperation::kBlur:
      value = filter.length() > 0
                  ? InterpolableLength::MaybeConvertCSSValue(filter.Item(0))
                  : InterpolableLength::CreateNeutral();
      break;

    case FilterOperation::kDropShadow:
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
    case FilterOperation::kGrayscale:
    case FilterOperation::kInvert:
    case FilterOperation::kSepia:
    case FilterOperation::kHueRotate:
      value = std::make_unique<InterpolableNumber>(0);
      break;

    case FilterOperation::kBrightness:
    case FilterOperation::kContrast:
    case FilterOperation::kOpacity:
    case FilterOperation::kSaturate:
      value = std::make_unique<InterpolableNumber>(1);
      break;

    case FilterOperation::kBlur:
      value = InterpolableLength::CreateNeutral();
      break;

    case FilterOperation::kDropShadow:
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
    case FilterOperation::kGrayscale:
    case FilterOperation::kHueRotate:
    case FilterOperation::kSaturate:
    case FilterOperation::kSepia: {
      double value =
          ClampParameter(To<InterpolableNumber>(*value_).Value(), type_);
      return MakeGarbageCollected<BasicColorMatrixFilterOperation>(value,
                                                                   type_);
    }

    case FilterOperation::kBrightness:
    case FilterOperation::kContrast:
    case FilterOperation::kInvert:
    case FilterOperation::kOpacity: {
      double value =
          ClampParameter(To<InterpolableNumber>(*value_).Value(), type_);
      return MakeGarbageCollected<BasicComponentTransferFilterOperation>(value,
                                                                         type_);
    }

    case FilterOperation::kBlur: {
      Length std_deviation = To<InterpolableLength>(*value_).CreateLength(
          state.CssToLengthConversionData(), Length::ValueRange::kNonNegative);
      return MakeGarbageCollected<BlurFilterOperation>(std_deviation);
    }

    case FilterOperation::kDropShadow: {
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
    case FilterOperation::kBrightness:
    case FilterOperation::kContrast:
    case FilterOperation::kGrayscale:
    case FilterOperation::kInvert:
    case FilterOperation::kOpacity:
    case FilterOperation::kSaturate:
    case FilterOperation::kSepia:
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
