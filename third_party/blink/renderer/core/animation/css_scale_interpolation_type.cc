// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_scale_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

InterpolableNumber* CSSValueToInterpolableNumber(const CSSValue& value) {
  if (auto* numeric = DynamicTo<CSSNumericLiteralValue>(value)) {
    return MakeGarbageCollected<InterpolableNumber>(numeric->ComputeNumber());
  }
  CHECK(value.IsMathFunctionValue());
  auto& function = To<CSSMathFunctionValue>(value);
  return MakeGarbageCollected<InterpolableNumber>(*function.ExpressionNode());
}

InterpolableValue* CreateScaleIdentity() {
  auto* list = MakeGarbageCollected<InterpolableList>(3);
  for (wtf_size_t i = 0; i < 3; i++)
    list->Set(i, MakeGarbageCollected<InterpolableNumber>(1));
  return list;
}

class InheritedScaleChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedScaleChecker(bool is_none, std::array<double, 3> scales)
      : is_none_(is_none), scales_(std::move(scales)) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    if (state.ParentStyle()->Scale()) {
      return state.ParentStyle()->Scale()->X() != scales_[0] &&
             state.ParentStyle()->Scale()->Y() != scales_[1] &&
             state.ParentStyle()->Scale()->Z() != scales_[2];
    }
    return is_none_;
  }

  bool is_none_;
  const std::array<double, 3> scales_;
};

}  // namespace

class CSSScaleNonInterpolableValue final : public NonInterpolableValue {
 public:
  ~CSSScaleNonInterpolableValue() final = default;

  static scoped_refptr<CSSScaleNonInterpolableValue> Create(
      const InterpolableList& list) {
    return base::AdoptRef(
        new CSSScaleNonInterpolableValue(list, list, false, false));
  }

  static scoped_refptr<CSSScaleNonInterpolableValue> CreateAdditive(
      const CSSScaleNonInterpolableValue& other) {
    const bool is_additive = true;
    return base::AdoptRef(new CSSScaleNonInterpolableValue(
        *other.start_, *other.end_, is_additive, is_additive));
  }

  static scoped_refptr<CSSScaleNonInterpolableValue> Merge(
      const CSSScaleNonInterpolableValue& start,
      const CSSScaleNonInterpolableValue& end) {
    return base::AdoptRef(new CSSScaleNonInterpolableValue(
        start.Start(), end.end(), start.IsStartAdditive(),
        end.IsEndAdditive()));
  }

  const InterpolableList& Start() const { return *start_; }
  const InterpolableList& end() const { return *end_; }
  bool IsStartAdditive() const { return is_start_additive_; }
  bool IsEndAdditive() const { return is_end_additive_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSScaleNonInterpolableValue(const InterpolableList& start,
                               const InterpolableList& end,
                               bool is_start_additive,
                               bool is_end_additive)
      : start_(start.Clone()),
        end_(end.Clone()),
        is_start_additive_(is_start_additive),
        is_end_additive_(is_end_additive) {}

  Persistent<const InterpolableList> start_;
  Persistent<const InterpolableList> end_;
  bool is_start_additive_;
  bool is_end_additive_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSScaleNonInterpolableValue);
template <>
struct DowncastTraits<CSSScaleNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSScaleNonInterpolableValue::static_type_;
  }
};

namespace {

InterpolationValue CreateInterpolationValue(ScaleTransformOperation* op) {
  if (!op) {
    return InterpolationValue(MakeGarbageCollected<InterpolableList>(0),
                              CSSScaleNonInterpolableValue::Create(
                                  *MakeGarbageCollected<InterpolableList>(0)));
  }

  auto* list = MakeGarbageCollected<InterpolableList>(3);
  list->Set(0, MakeGarbageCollected<InterpolableNumber>(op->X()));
  list->Set(1, MakeGarbageCollected<InterpolableNumber>(op->Y()));
  list->Set(2, MakeGarbageCollected<InterpolableNumber>(op->Z()));
  return InterpolationValue(list, CSSScaleNonInterpolableValue::Create(*list));
}

InterpolationValue CreateInterpolationValue(std::array<double, 3> a) {
  auto* list = MakeGarbageCollected<InterpolableList>(3);
  list->Set(0, MakeGarbageCollected<InterpolableNumber>(a[0]));
  list->Set(1, MakeGarbageCollected<InterpolableNumber>(a[1]));
  list->Set(2, MakeGarbageCollected<InterpolableNumber>(a[2]));
  return InterpolationValue(list, CSSScaleNonInterpolableValue::Create(*list));
}

InterpolationValue CreateInterpolationValue(
    std::array<InterpolableNumber*, 3> a) {
  auto* list = MakeGarbageCollected<InterpolableList>(3);
  list->Set(0, a[0]);
  list->Set(1, a[1]);
  list->Set(2, a[2]);
  return InterpolationValue(list, CSSScaleNonInterpolableValue::Create(*list));
}

InterpolationValue CreateInterpolationValue() {
  auto* list = MakeGarbageCollected<InterpolableList>(3);
  list->Set(0, MakeGarbageCollected<InterpolableNumber>(1.0));
  list->Set(1, MakeGarbageCollected<InterpolableNumber>(1.0));
  list->Set(2, MakeGarbageCollected<InterpolableNumber>(1.0));
  return InterpolationValue(MakeGarbageCollected<InterpolableList>(0),
                            CSSScaleNonInterpolableValue::Create(*list));
}

}  // namespace

InterpolationValue CSSScaleInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return CreateInterpolationValue({1.0, 1.0, 1.0});
}

InterpolationValue CSSScaleInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return CreateInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  ScaleTransformOperation* op = state.ParentStyle()->Scale();
  double x = op ? op->X() : 1.0;
  double y = op ? op->Y() : 1.0;
  double z = op ? op->Z() : 1.0;
  conversion_checkers.push_back(MakeGarbageCollected<InheritedScaleChecker>(
      !op, std::array<double, 3>({x, y, z})));
  return CreateInterpolationValue(op);
}

InterpolationValue CSSScaleInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList())
    return CreateInterpolationValue();

  const auto& list = To<CSSValueList>(value);
  DCHECK(list.length() >= 1 && list.length() <= 3);

  if (list.length() == 1) {
    InterpolableNumber* scale = CSSValueToInterpolableNumber(list.Item(0));
    // single value defines a 2d scale according to the spec
    // see https://drafts.csswg.org/css-transforms-2/#propdef-scale
    return CreateInterpolationValue(
        {scale, scale, MakeGarbageCollected<InterpolableNumber>(1.0)});
  } else if (list.length() == 2) {
    InterpolableNumber* x_scale = CSSValueToInterpolableNumber(list.Item(0));
    InterpolableNumber* y_scale = CSSValueToInterpolableNumber(list.Item(1));
    return CreateInterpolationValue(
        {x_scale, y_scale, MakeGarbageCollected<InterpolableNumber>(1.0)});
  } else {
    InterpolableNumber* x_scale = CSSValueToInterpolableNumber(list.Item(0));
    InterpolableNumber* y_scale = CSSValueToInterpolableNumber(list.Item(1));
    InterpolableNumber* z_scale = CSSValueToInterpolableNumber(list.Item(2));
    return CreateInterpolationValue({x_scale, y_scale, z_scale});
  }
}

InterpolationValue CSSScaleInterpolationType::PreInterpolationCompositeIfNeeded(
    InterpolationValue value,
    const InterpolationValue& underlying,
    EffectModel::CompositeOperation,
    ConversionCheckers&) const {
  value.non_interpolable_value = CSSScaleNonInterpolableValue::CreateAdditive(
      To<CSSScaleNonInterpolableValue>(*value.non_interpolable_value));
  return value;
}

PairwiseInterpolationValue CSSScaleInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  wtf_size_t start_list_length =
      To<InterpolableList>(*start.interpolable_value).length();
  wtf_size_t end_list_length =
      To<InterpolableList>(*end.interpolable_value).length();
  if (start_list_length < end_list_length)
    start.interpolable_value = CreateScaleIdentity();
  else if (end_list_length < start_list_length)
    end.interpolable_value = CreateScaleIdentity();

  return PairwiseInterpolationValue(
      std::move(start.interpolable_value), std::move(end.interpolable_value),
      CSSScaleNonInterpolableValue::Merge(
          To<CSSScaleNonInterpolableValue>(*start.non_interpolable_value),
          To<CSSScaleNonInterpolableValue>(*end.non_interpolable_value)));
}

InterpolationValue
CSSScaleInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateInterpolationValue(style.Scale());
}

void CSSScaleInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (To<InterpolableList>(
          *underlying_value_owner.MutableValue().interpolable_value)
          .length() == 0) {
    underlying_value_owner.MutableValue().interpolable_value =
        CreateScaleIdentity();
  }

  const auto& metadata =
      To<CSSScaleNonInterpolableValue>(*value.non_interpolable_value);
  DCHECK(metadata.IsStartAdditive() || metadata.IsEndAdditive());

  auto& underlying_list = To<InterpolableList>(
      *underlying_value_owner.MutableValue().interpolable_value);
  for (wtf_size_t i = 0; i < 3; i++) {
    auto& underlying = To<InterpolableNumber>(*underlying_list.GetMutable(i));

    InterpolableNumber& start_number =
        metadata.IsStartAdditive()
            ? *underlying.Clone()
            : *MakeGarbageCollected<InterpolableNumber>(1.0);
    start_number.Scale(*To<InterpolableNumber>(metadata.Start().Get(i)));
    InterpolableNumber& end_number =
        metadata.IsEndAdditive()
            ? *underlying.Clone()
            : *MakeGarbageCollected<InterpolableNumber>(1.0);
    end_number.Scale(*To<InterpolableNumber>(metadata.end().Get(i)));
    start_number.Interpolate(end_number, interpolation_fraction, underlying);
  }
}

void CSSScaleInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  auto& list = To<InterpolableList>(interpolable_value);
  if (!list.length()) {
    state.StyleBuilder().SetScale(nullptr);
    return;
  }
  state.StyleBuilder().SetScale(MakeGarbageCollected<ScaleTransformOperation>(
      To<InterpolableNumber>(list.Get(0))
          ->Value(state.CssToLengthConversionData()),
      To<InterpolableNumber>(list.Get(1))
          ->Value(state.CssToLengthConversionData()),
      To<InterpolableNumber>(list.Get(2))
          ->Value(state.CssToLengthConversionData()),
      TransformOperation::kScale3D));
}

}  // namespace blink
