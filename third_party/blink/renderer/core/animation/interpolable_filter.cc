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
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

InterpolableNumber* ConvertFilterOperationToInterpolableNumber(
    CSSValueID type,
    const CSSValue& item) {
  switch (type) {
    case CSSValueID::kGrayscale:
    case CSSValueID::kSepia:
    case CSSValueID::kSaturate:
    case CSSValueID::kInvert:
    case CSSValueID::kBrightness:
    case CSSValueID::kContrast:
    case CSSValueID::kOpacity: {
      const CSSPrimitiveValue& value = To<CSSPrimitiveValue>(item);
      if (value.IsPercentage()) {
        return MakeGarbageCollected<InterpolableNumber>(
            *value.ConvertLiteralsFromPercentageToNumber());
      }
      return MakeGarbageCollected<InterpolableNumber>(value);
    }
    case CSSValueID::kHueRotate: {
      return MakeGarbageCollected<InterpolableNumber>(
          To<CSSPrimitiveValue>(item));
    }
    default:
      return MakeGarbageCollected<InterpolableNumber>(0);
  }
}

InterpolableNumber* CreateDefaultValue(CSSValueID type) {
  // See https://www.w3.org/TR/filter-effects-1/#filter-functions for the
  // mapping of OperationType to initial value.
  switch (type) {
    case CSSValueID::kGrayscale:
    case CSSValueID::kSepia:
    case CSSValueID::kSaturate:
    case CSSValueID::kInvert:
    case CSSValueID::kBrightness:
    case CSSValueID::kContrast:
    case CSSValueID::kOpacity:
      return MakeGarbageCollected<InterpolableNumber>(1);
    case CSSValueID::kHueRotate:
      return MakeGarbageCollected<InterpolableNumber>(
          0, CSSPrimitiveValue::UnitType::kDegrees);
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

// static
InterpolableFilter* InterpolableFilter::MaybeCreate(
    const FilterOperation& filter,
    const CSSProperty& property,
    double zoom,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) {
  InterpolableValue* value = nullptr;
  FilterOperation::OperationType type = filter.GetType();
  switch (type) {
    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kSaturate:
    case FilterOperation::OperationType::kSepia:
      value = MakeGarbageCollected<InterpolableNumber>(
          To<BasicColorMatrixFilterOperation>(filter).Amount(),
          CSSPrimitiveValue::UnitType::kNumber);
      break;

    case FilterOperation::OperationType::kHueRotate:
      value = MakeGarbageCollected<InterpolableNumber>(
          To<BasicColorMatrixFilterOperation>(filter).Amount(),
          CSSPrimitiveValue::UnitType::kDegrees);
      break;

    case FilterOperation::OperationType::kBrightness:
      value = MakeGarbageCollected<InterpolableNumber>(
          To<BasicComponentTransferFilterOperation>(filter).Amount());
      break;

    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kOpacity:
      value = MakeGarbageCollected<InterpolableNumber>(
          To<BasicComponentTransferFilterOperation>(filter).Amount(),
          CSSPrimitiveValue::UnitType::kNumber);
      break;

    case FilterOperation::OperationType::kBlur:
      value = InterpolableLength::MaybeConvertLength(
          To<BlurFilterOperation>(filter).StdDeviation(), property, zoom,
          /*interpolate_size=*/std::nullopt);
      break;

    case FilterOperation::OperationType::kDropShadow:
      value = InterpolableShadow::Create(
          To<DropShadowFilterOperation>(filter).Shadow(), zoom, color_scheme,
          color_provider);
      break;

    case FilterOperation::OperationType::kReference:
      return nullptr;

    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }

  if (!value)
    return nullptr;
  return MakeGarbageCollected<InterpolableFilter>(std::move(value), type);
}

// static
InterpolableFilter* InterpolableFilter::MaybeConvertCSSValue(
    const CSSValue& css_value,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) {
  if (css_value.IsURIValue())
    return nullptr;

  const auto& filter = To<CSSFunctionValue>(css_value);
  DCHECK_LE(filter.length(), 1u);

  InterpolableValue* value = nullptr;
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
      value = filter.length() > 0 ? ConvertFilterOperationToInterpolableNumber(
                                        filter.FunctionType(), filter.Item(0))
                                  : CreateDefaultValue(filter.FunctionType());
      break;

    case FilterOperation::OperationType::kBlur:
      value = filter.length() > 0
                  ? InterpolableLength::MaybeConvertCSSValue(filter.Item(0))
                  : InterpolableLength::CreateNeutral();
      break;

    case FilterOperation::OperationType::kDropShadow:
      value = InterpolableShadow::MaybeConvertCSSValue(
          filter.Item(0), color_scheme, color_provider);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }

  if (!value)
    return nullptr;
  return MakeGarbageCollected<InterpolableFilter>(value, type);
}

// static
InterpolableFilter* InterpolableFilter::CreateInitialValue(
    FilterOperation::OperationType type) {
  // See https://www.w3.org/TR/filter-effects-1/#filter-functions for the
  // mapping of OperationType to initial value for interpolation.
  InterpolableValue* value = nullptr;
  switch (type) {
    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kSepia:
      value = MakeGarbageCollected<InterpolableNumber>(0);
      break;

    case FilterOperation::OperationType::kHueRotate:
      value = MakeGarbageCollected<InterpolableNumber>(
          0, CSSPrimitiveValue::UnitType::kDegrees);
      break;

    case FilterOperation::OperationType::kBrightness:
    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kOpacity:
    case FilterOperation::OperationType::kSaturate:
      value = MakeGarbageCollected<InterpolableNumber>(1);
      break;

    case FilterOperation::OperationType::kBlur:
      value = InterpolableLength::CreateNeutral();
      break;

    case FilterOperation::OperationType::kDropShadow:
      value = InterpolableShadow::CreateNeutral();
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }

  return MakeGarbageCollected<InterpolableFilter>(value, type);
}

FilterOperation* InterpolableFilter::CreateFilterOperation(
    const StyleResolverState& state) const {
  switch (type_) {
    case FilterOperation::OperationType::kGrayscale:
    case FilterOperation::OperationType::kHueRotate:
    case FilterOperation::OperationType::kSaturate:
    case FilterOperation::OperationType::kSepia: {
      double value = ClampParameter(To<InterpolableNumber>(*value_).Value(
                                        state.CssToLengthConversionData()),
                                    type_);
      return MakeGarbageCollected<BasicColorMatrixFilterOperation>(value,
                                                                   type_);
    }

    case FilterOperation::OperationType::kBrightness:
    case FilterOperation::OperationType::kContrast:
    case FilterOperation::OperationType::kInvert:
    case FilterOperation::OperationType::kOpacity: {
      double value = ClampParameter(To<InterpolableNumber>(*value_).Value(
                                        state.CssToLengthConversionData()),
                                    type_);
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
      return MakeGarbageCollected<DropShadowFilterOperation>(shadow_data);
    }

    default:
      NOTREACHED_IN_MIGRATION();
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
      value_->Add(*MakeGarbageCollected<InterpolableNumber>(-1));
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
