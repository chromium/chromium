// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_scale_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/length_units_checker.h"
#include "third_party/blink/renderer/core/animation/tree_counting_checker.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

InterpolableNumber* CSSValueToInterpolableNumber(
    const CSSValue& value,
    const CSSLengthResolver& length_resolver) {
  const auto& primitive_value = To<CSSPrimitiveValue>(value);
  // TODO(crbug.com/41494232): Don't resolve it here, once we can divide units.
  // The problem now is when we end up with kNumber for neutral keyframe
  // and kPercentage for non-neutral keyframe, we have to sum number and
  // percentage, which is not allowed (context: crbug.com/396584141).
  return MakeGarbageCollected<InterpolableNumber>(
      primitive_value.ComputeNumber(length_resolver));
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
  CSSScaleNonInterpolableValue(const InterpolableList& start,
                               const InterpolableList& end,
                               bool is_start_additive = false,
                               bool is_end_additive = false)
      : start_(start.Clone()),
        end_(end.Clone()),
        is_start_additive_(is_start_additive),
        is_end_additive_(is_end_additive) {}

  ~CSSScaleNonInterpolableValue() final = default;

  void Trace(Visitor* visitor) const override {
    NonInterpolableValue::Trace(visitor);
    visitor->Trace(start_);
    visitor->Trace(end_);
  }

  static CSSScaleNonInterpolableValue* Merge(
      const CSSScaleNonInterpolableValue& start,
      const CSSScaleNonInterpolableValue& end) {
    return MakeGarbageCollected<CSSScaleNonInterpolableValue>(
        start.Start(), end.End(), start.IsStartAdditive(), end.IsEndAdditive());
  }

  const InterpolableList& Start() const { return *start_; }
  const InterpolableList& End() const { return *end_; }
  bool IsStartAdditive() const { return is_start_additive_; }
  bool IsEndAdditive() const { return is_end_additive_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  Member<const InterpolableList> start_;
  Member<const InterpolableList> end_;
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
    return InterpolationValue(
        MakeGarbageCollected<InterpolableList>(0),
        MakeGarbageCollected<CSSScaleNonInterpolableValue>(
            *MakeGarbageCollected<InterpolableList>(0),
            *MakeGarbageCollected<InterpolableList>(0)));
  }

  auto* list = MakeGarbageCollected<InterpolableList>(3);
  list->Set(0, MakeGarbageCollected<InterpolableNumber>(op->X()));
  list->Set(1, MakeGarbageCollected<InterpolableNumber>(op->Y()));
  list->Set(2, MakeGarbageCollected<InterpolableNumber>(op->Z()));
  return InterpolationValue(
      list, MakeGarbageCollected<CSSScaleNonInterpolableValue>(*list, *list));
}

InterpolationValue CreateInterpolationValue(std::array<double, 3> a) {
  auto* list = MakeGarbageCollected<InterpolableList>(3);
  list->Set(0, MakeGarbageCollected<InterpolableNumber>(a[0]));
  list->Set(1, MakeGarbageCollected<InterpolableNumber>(a[1]));
  list->Set(2, MakeGarbageCollected<InterpolableNumber>(a[2]));
  return InterpolationValue(
      list, MakeGarbageCollected<CSSScaleNonInterpolableValue>(*list, *list));
}

InterpolationValue CreateInterpolationValue(
    std::array<InterpolableNumber*, 3> a) {
  auto* list = MakeGarbageCollected<InterpolableList>(3);
  list->Set(0, a[0]);
  list->Set(1, a[1]);
  list->Set(2, a[2]);
  return InterpolationValue(
      list, MakeGarbageCollected<CSSScaleNonInterpolableValue>(*list, *list));
}

InterpolationValue CreateInterpolationValue() {
  auto* list = MakeGarbageCollected<InterpolableList>(3);
  list->Set(0, MakeGarbageCollected<InterpolableNumber>(1.0));
  list->Set(1, MakeGarbageCollected<InterpolableNumber>(1.0));
  list->Set(2, MakeGarbageCollected<InterpolableNumber>(1.0));
  return InterpolationValue(
      MakeGarbageCollected<InterpolableList>(0),
      MakeGarbageCollected<CSSScaleNonInterpolableValue>(*list, *list));
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
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!value.IsBaseValueList())
    return CreateInterpolationValue();

  const auto& list = To<CSSValueList>(value);
  DCHECK(list.length() >= 1 && list.length() <= 3);

  CSSToLengthConversionData conversion_data = state.CssToLengthConversionData();

  CSSPrimitiveValue::LengthTypeFlags types;
  for (const auto& scale_value : list) {
    const auto& primitive_value = To<CSSPrimitiveValue>(*scale_value);
    primitive_value.AccumulateLengthUnitTypes(types);
    if (primitive_value.IsElementDependent()) {
      conversion_checkers.push_back(
          TreeCountingChecker::Create(conversion_data));
      break;
    }
  }
  if (InterpolationType::ConversionChecker* length_units_checker =
          LengthUnitsChecker::MaybeCreate(types, state)) {
    conversion_checkers.push_back(length_units_checker);
  }
  if (list.length() == 1) {
    InterpolableNumber* scale =
        CSSValueToInterpolableNumber(list.Item(0), conversion_data);
    // single value defines a 2d scale according to the spec
    // see https://drafts.csswg.org/css-transforms-2/#propdef-scale
    return CreateInterpolationValue(
        {scale, scale, MakeGarbageCollected<InterpolableNumber>(1.0)});
  } else if (list.length() == 2) {
    InterpolableNumber* x_scale =
        CSSValueToInterpolableNumber(list.Item(0), conversion_data);
    InterpolableNumber* y_scale =
        CSSValueToInterpolableNumber(list.Item(1), conversion_data);
    return CreateInterpolationValue(
        {x_scale, y_scale, MakeGarbageCollected<InterpolableNumber>(1.0)});
  } else {
    InterpolableNumber* x_scale =
        CSSValueToInterpolableNumber(list.Item(0), conversion_data);
    InterpolableNumber* y_scale =
        CSSValueToInterpolableNumber(list.Item(1), conversion_data);
    InterpolableNumber* z_scale =
        CSSValueToInterpolableNumber(list.Item(2), conversion_data);
    return CreateInterpolationValue({x_scale, y_scale, z_scale});
  }
}

InterpolationValue CSSScaleInterpolationType::PreInterpolationCompositeIfNeeded(
    InterpolationValue value,
    const InterpolationValue& underlying,
    EffectModel::CompositeOperation,
    ConversionCheckers&) const {
  const auto& other =
      To<CSSScaleNonInterpolableValue>(*value.non_interpolable_value);
  value.non_interpolable_value =
      MakeGarbageCollected<CSSScaleNonInterpolableValue>(
          other.Start(), other.End(), /* is_additive */ true,
          /* is_additive */ true);
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
    end_number.Scale(*To<InterpolableNumber>(metadata.End().Get(i)));
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
