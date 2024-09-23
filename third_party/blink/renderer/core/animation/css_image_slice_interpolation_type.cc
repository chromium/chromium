// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_image_slice_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/image_slice_property_functions.h"
#include "third_party/blink/renderer/core/animation/side_index.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"

namespace blink {

namespace {

struct SliceTypes {
  explicit SliceTypes(const ImageSlice& slice) {
    is_number[kSideTop] = slice.slices.Top().IsFixed();
    is_number[kSideRight] = slice.slices.Right().IsFixed();
    is_number[kSideBottom] = slice.slices.Bottom().IsFixed();
    is_number[kSideLeft] = slice.slices.Left().IsFixed();
    fill = slice.fill;
  }
  explicit SliceTypes(const cssvalue::CSSBorderImageSliceValue& slice) {
    auto* top_primitive_value =
        DynamicTo<CSSPrimitiveValue>(slice.Slices().Top());
    is_number[kSideTop] =
        top_primitive_value && top_primitive_value->IsNumber();

    auto* right_primitive_value =
        DynamicTo<CSSPrimitiveValue>(slice.Slices().Right());
    is_number[kSideRight] =
        right_primitive_value && right_primitive_value->IsNumber();

    auto* bottom_primitive_value =
        DynamicTo<CSSPrimitiveValue>(slice.Slices().Bottom());
    is_number[kSideBottom] =
        bottom_primitive_value && bottom_primitive_value->IsNumber();

    auto* left_primitive_value =
        DynamicTo<CSSPrimitiveValue>(slice.Slices().Left());
    is_number[kSideLeft] =
        left_primitive_value && left_primitive_value->IsNumber();

    fill = slice.Fill();
  }

  bool operator==(const SliceTypes& other) const {
    for (size_t i = 0; i < kSideIndexCount; i++) {
      if (is_number[i] != other.is_number[i])
        return false;
    }
    return fill == other.fill;
  }
  bool operator!=(const SliceTypes& other) const { return !(*this == other); }

  // If a side is not a number then it is a percentage.
  std::array<bool, kSideIndexCount> is_number;
  bool fill;
};

}  // namespace

class CSSImageSliceNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<CSSImageSliceNonInterpolableValue> Create(
      const SliceTypes& types) {
    return base::AdoptRef(new CSSImageSliceNonInterpolableValue(types));
  }

  const SliceTypes& Types() const { return types_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSImageSliceNonInterpolableValue(const SliceTypes& types) : types_(types) {}

  const SliceTypes types_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSImageSliceNonInterpolableValue);
template <>
struct DowncastTraits<CSSImageSliceNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSImageSliceNonInterpolableValue::static_type_;
  }
};

namespace {

class UnderlyingSliceTypesChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingSliceTypesChecker(const SliceTypes& underlying_types)
      : underlying_types_(underlying_types) {}

  static SliceTypes GetUnderlyingSliceTypes(
      const InterpolationValue& underlying) {
    return To<CSSImageSliceNonInterpolableValue>(
               *underlying.non_interpolable_value)
        .Types();
  }

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return underlying_types_ == GetUnderlyingSliceTypes(underlying);
  }

  const SliceTypes underlying_types_;
};

class InheritedSliceTypesChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedSliceTypesChecker(const CSSProperty& property,
                             const SliceTypes& inherited_types)
      : property_(property), inherited_types_(inherited_types) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return inherited_types_ ==
           SliceTypes(ImageSlicePropertyFunctions::GetImageSlice(
               property_, *state.ParentStyle()));
  }

  const CSSProperty& property_;
  const SliceTypes inherited_types_;
};

InterpolationValue ConvertImageSlice(const ImageSlice& slice, double zoom) {
  auto* list = MakeGarbageCollected<InterpolableList>(kSideIndexCount);
  std::array<const Length*, kSideIndexCount> sides{};
  sides[kSideTop] = &slice.slices.Top();
  sides[kSideRight] = &slice.slices.Right();
  sides[kSideBottom] = &slice.slices.Bottom();
  sides[kSideLeft] = &slice.slices.Left();

  for (wtf_size_t i = 0; i < kSideIndexCount; i++) {
    const Length& side = *sides[i];
    list->Set(i,
              MakeGarbageCollected<InterpolableNumber>(
                  side.IsFixed() ? side.Pixels() / zoom : side.Percent(),
                  side.IsFixed() ? CSSPrimitiveValue::UnitType::kNumber
                                 : CSSPrimitiveValue::UnitType::kPercentage));
  }

  return InterpolationValue(
      std::move(list),
      CSSImageSliceNonInterpolableValue::Create(SliceTypes(slice)));
}

}  // namespace

InterpolationValue CSSImageSliceInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  SliceTypes underlying_types =
      UnderlyingSliceTypesChecker::GetUnderlyingSliceTypes(underlying);
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingSliceTypesChecker>(underlying_types));
  LengthBox zero_box(
      underlying_types.is_number[kSideTop] ? Length::Fixed(0)
                                           : Length::Percent(0),
      underlying_types.is_number[kSideRight] ? Length::Fixed(0)
                                             : Length::Percent(0),
      underlying_types.is_number[kSideBottom] ? Length::Fixed(0)
                                              : Length::Percent(0),
      underlying_types.is_number[kSideLeft] ? Length::Fixed(0)
                                            : Length::Percent(0));
  return ConvertImageSlice(ImageSlice(zero_box, underlying_types.fill), 1);
}

InterpolationValue CSSImageSliceInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  return ConvertImageSlice(
      ImageSlicePropertyFunctions::GetInitialImageSlice(
          CssProperty(), state.GetDocument().GetStyleResolver().InitialStyle()),
      1);
}

InterpolationValue CSSImageSliceInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const ImageSlice& inherited_image_slice =
      ImageSlicePropertyFunctions::GetImageSlice(CssProperty(),
                                                 *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedSliceTypesChecker>(
          CssProperty(), SliceTypes(inherited_image_slice)));
  return ConvertImageSlice(inherited_image_slice,
                           state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSImageSliceInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!IsA<cssvalue::CSSBorderImageSliceValue>(value))
    return nullptr;

  const cssvalue::CSSBorderImageSliceValue& slice =
      To<cssvalue::CSSBorderImageSliceValue>(value);
  auto* list = MakeGarbageCollected<InterpolableList>(kSideIndexCount);
  std::array<const CSSValue*, kSideIndexCount> sides;
  sides[kSideTop] = slice.Slices().Top();
  sides[kSideRight] = slice.Slices().Right();
  sides[kSideBottom] = slice.Slices().Bottom();
  sides[kSideLeft] = slice.Slices().Left();

  for (wtf_size_t i = 0; i < kSideIndexCount; i++) {
    const auto& side = *To<CSSPrimitiveValue>(sides[i]);
    DCHECK(side.IsNumber() || side.IsPercentage());
    if (auto* numeric_value = DynamicTo<CSSNumericLiteralValue>(side)) {
      CSSPrimitiveValue::UnitType unit_type =
          numeric_value->IsNumber() ? CSSPrimitiveValue::UnitType::kNumber
                                    : CSSPrimitiveValue::UnitType::kPercentage;
      list->Set(
          i, MakeGarbageCollected<InterpolableNumber>(
                 numeric_value->IsNumber() ? numeric_value->ComputeNumber()
                                           : numeric_value->ComputePercentage(),
                 unit_type));
      continue;
    }
    CHECK(side.IsMathFunctionValue());
    list->Set(i, MakeGarbageCollected<InterpolableNumber>(
                     *To<CSSMathFunctionValue>(side).ExpressionNode()));
  }

  return InterpolationValue(
      list, CSSImageSliceNonInterpolableValue::Create(SliceTypes(slice)));
}

InterpolationValue
CSSImageSliceInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertImageSlice(
      ImageSlicePropertyFunctions::GetImageSlice(CssProperty(), style),
      style.EffectiveZoom());
}

PairwiseInterpolationValue CSSImageSliceInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const auto& start_slice_types =
      To<CSSImageSliceNonInterpolableValue>(*start.non_interpolable_value)
          .Types();
  const auto& end_slice_types =
      To<CSSImageSliceNonInterpolableValue>(*end.non_interpolable_value)
          .Types();

  if (start_slice_types != end_slice_types)
    return nullptr;

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

void CSSImageSliceInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const auto& underlying_types =
      To<CSSImageSliceNonInterpolableValue>(
          *underlying_value_owner.Value().non_interpolable_value)
          .Types();
  const auto& types =
      To<CSSImageSliceNonInterpolableValue>(*value.non_interpolable_value)
          .Types();

  if (underlying_types == types)
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  else
    underlying_value_owner.Set(*this, value);
}

void CSSImageSliceInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  const auto& list = To<InterpolableList>(interpolable_value);
  const auto& types =
      To<CSSImageSliceNonInterpolableValue>(non_interpolable_value)->Types();
  const auto& convert_side = [&types, &list, &builder,
                              &state](wtf_size_t index) {
    float value = ClampTo<float>(To<InterpolableNumber>(list.Get(index))
                                     ->Value(state.CssToLengthConversionData()),
                                 0);
    return types.is_number[index]
               ? Length::Fixed(value * builder.EffectiveZoom())
               : Length::Percent(value);
  };
  LengthBox box(convert_side(kSideTop), convert_side(kSideRight),
                convert_side(kSideBottom), convert_side(kSideLeft));
  ImageSlicePropertyFunctions::SetImageSlice(CssProperty(), builder,
                                             ImageSlice(box, types.fill));
}

}  // namespace blink
