// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/length_interpolation_functions.h"

#include "third_party/blink/renderer/core/css/css_calculation_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

// This class is implemented as a singleton whose instance represents the
// presence of percentages being used in a Length value while nullptr represents
// the absence of any percentages.
class CSSLengthNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSLengthNonInterpolableValue() final { NOTREACHED(); }
  static scoped_refptr<CSSLengthNonInterpolableValue> Create(
      bool has_percentage) {
    DEFINE_STATIC_REF(CSSLengthNonInterpolableValue, singleton,
                      base::AdoptRef(new CSSLengthNonInterpolableValue()));
    DCHECK(singleton);
    return has_percentage ? singleton : nullptr;
  }
  static scoped_refptr<CSSLengthNonInterpolableValue> Merge(
      const NonInterpolableValue* a,
      const NonInterpolableValue* b) {
    return Create(HasPercentage(a) || HasPercentage(b));
  }
  static bool HasPercentage(const NonInterpolableValue*);

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSLengthNonInterpolableValue() = default;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSLengthNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSLengthNonInterpolableValue);

bool CSSLengthNonInterpolableValue::HasPercentage(
    const NonInterpolableValue* non_interpolable_value) {
  DCHECK(IsCSSLengthNonInterpolableValue(non_interpolable_value));
  return static_cast<bool>(non_interpolable_value);
}

std::unique_ptr<InterpolableValue>
LengthInterpolationFunctions::CreateInterpolablePixels(double pixels) {
  std::unique_ptr<InterpolableList> interpolable_list =
      CreateNeutralInterpolableValue();
  interpolable_list->Set(CSSPrimitiveValue::kUnitTypePixels,
                         InterpolableNumber::Create(pixels));
  return std::move(interpolable_list);
}

InterpolationValue LengthInterpolationFunctions::CreateInterpolablePercent(
    double percent) {
  std::unique_ptr<InterpolableList> interpolable_list =
      CreateNeutralInterpolableValue();
  interpolable_list->Set(CSSPrimitiveValue::kUnitTypePercentage,
                         InterpolableNumber::Create(percent));
  return InterpolationValue(std::move(interpolable_list),
                            CSSLengthNonInterpolableValue::Create(true));
}

std::unique_ptr<InterpolableList>
LengthInterpolationFunctions::CreateNeutralInterpolableValue() {
  const size_t kLength = CSSPrimitiveValue::kLengthUnitTypeCount;
  std::unique_ptr<InterpolableList> values = InterpolableList::Create(kLength);
  for (wtf_size_t i = 0; i < kLength; i++)
    values->Set(i, InterpolableNumber::Create(0));
  return values;
}

InterpolationValue LengthInterpolationFunctions::MaybeConvertCSSValue(
    const CSSValue& value) {
  if (!value.IsPrimitiveValue())
    return nullptr;

  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
  if (!primitive_value.IsLength() && !primitive_value.IsPercentage() &&
      !primitive_value.IsCalculatedPercentageWithLength())
    return nullptr;

  CSSLengthArray length_array;
  primitive_value.AccumulateLengthArray(length_array);

  std::unique_ptr<InterpolableList> values =
      InterpolableList::Create(CSSPrimitiveValue::kLengthUnitTypeCount);
  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; i++)
    values->Set(i, InterpolableNumber::Create(length_array.values[i]));

  bool has_percentage =
      length_array.type_flags.Get(CSSPrimitiveValue::kUnitTypePercentage);
  return InterpolationValue(
      std::move(values), CSSLengthNonInterpolableValue::Create(has_percentage));
}

InterpolationValue LengthInterpolationFunctions::MaybeConvertLength(
    const Length& length,
    float zoom) {
  if (!length.IsSpecified())
    return nullptr;

  PixelsAndPercent pixels_and_percent = length.GetPixelsAndPercent();
  std::unique_ptr<InterpolableList> values = CreateNeutralInterpolableValue();
  values->Set(CSSPrimitiveValue::kUnitTypePixels,
              InterpolableNumber::Create(pixels_and_percent.pixels / zoom));
  values->Set(CSSPrimitiveValue::kUnitTypePercentage,
              InterpolableNumber::Create(pixels_and_percent.percent));

  return InterpolationValue(
      std::move(values),
      CSSLengthNonInterpolableValue::Create(length.IsPercentOrCalc()));
}

PairwiseInterpolationValue LengthInterpolationFunctions::MergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  return PairwiseInterpolationValue(
      std::move(start.interpolable_value), std::move(end.interpolable_value),
      CSSLengthNonInterpolableValue::Merge(start.non_interpolable_value.get(),
                                           end.non_interpolable_value.get()));
}

bool LengthInterpolationFunctions::NonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  DCHECK(IsCSSLengthNonInterpolableValue(a));
  DCHECK(IsCSSLengthNonInterpolableValue(b));
  return true;
}

void LengthInterpolationFunctions::Composite(
    std::unique_ptr<InterpolableValue>& underlying_interpolable_value,
    scoped_refptr<NonInterpolableValue>& underlying_non_interpolable_value,
    double underlying_fraction,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) {
  underlying_interpolable_value->ScaleAndAdd(underlying_fraction,
                                             interpolable_value);
  underlying_non_interpolable_value = CSSLengthNonInterpolableValue::Merge(
      underlying_non_interpolable_value.get(), non_interpolable_value);
}

void LengthInterpolationFunctions::SubtractFromOneHundredPercent(
    InterpolationValue& result) {
  InterpolableList& list = ToInterpolableList(*result.interpolable_value);
  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; i++) {
    double value = -ToInterpolableNumber(*list.Get(i)).Value();
    if (i == CSSPrimitiveValue::kUnitTypePercentage)
      value += 100;
    ToInterpolableNumber(*list.GetMutable(i)).Set(value);
  }
  result.non_interpolable_value = CSSLengthNonInterpolableValue::Create(true);
}

static double ClampToRange(double x, ValueRange range) {
  return (range == kValueRangeNonNegative && x < 0) ? 0 : x;
}

CSSPrimitiveValue::UnitType IndexToUnitType(size_t index) {
  return CSSPrimitiveValue::LengthUnitTypeToUnitType(
      static_cast<CSSPrimitiveValue::LengthUnitType>(index));
}

Length LengthInterpolationFunctions::CreateLength(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const CSSToLengthConversionData& conversion_data,
    ValueRange range) {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  bool has_percentage =
      CSSLengthNonInterpolableValue::HasPercentage(non_interpolable_value);
  double pixels = 0;
  double percentage = 0;
  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; i++) {
    double value = ToInterpolableNumber(*interpolable_list.Get(i)).Value();
    if (value == 0)
      continue;
    if (i == CSSPrimitiveValue::kUnitTypePercentage) {
      percentage = value;
    } else {
      pixels += conversion_data.ZoomedComputedPixels(value, IndexToUnitType(i));
    }
  }

  if (percentage != 0)
    has_percentage = true;
  if (pixels != 0 && has_percentage)
    return Length(
        CalculationValue::Create(PixelsAndPercent(pixels, percentage), range));
  if (has_percentage)
    return Length(ClampToRange(percentage, range), kPercent);
  return Length(
      CSSPrimitiveValue::ClampToCSSLengthRange(ClampToRange(pixels, range)),
      kFixed);
}

const CSSValue* LengthInterpolationFunctions::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    ValueRange range) {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  bool has_percentage =
      CSSLengthNonInterpolableValue::HasPercentage(non_interpolable_value);

  CSSCalcExpressionNode* root_node = nullptr;
  CSSPrimitiveValue* first_value = nullptr;

  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; i++) {
    double value = ToInterpolableNumber(*interpolable_list.Get(i)).Value();
    if (value == 0 &&
        (i != CSSPrimitiveValue::kUnitTypePercentage || !has_percentage)) {
      continue;
    }
    CSSPrimitiveValue* current_value =
        CSSPrimitiveValue::Create(value, IndexToUnitType(i));

    if (!first_value) {
      DCHECK(!root_node);
      first_value = current_value;
      continue;
    }
    CSSCalcExpressionNode* current_node =
        CSSCalcValue::CreateExpressionNode(current_value);
    if (!root_node) {
      root_node = CSSCalcValue::CreateExpressionNode(first_value);
    }
    root_node =
        CSSCalcValue::CreateExpressionNode(root_node, current_node, kCalcAdd);
  }

  if (root_node) {
    return CSSPrimitiveValue::Create(CSSCalcValue::Create(root_node));
  }
  if (first_value) {
    return first_value;
  }
  return CSSPrimitiveValue::Create(0, CSSPrimitiveValue::UnitType::kPixels);
}

}  // namespace blink
