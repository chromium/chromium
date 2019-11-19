// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_scale_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

struct Scale {
  Scale(double x, double y, double z) { Init(x, y, z, false); }
  explicit Scale() { Init(1, 1, 1, true); }
  explicit Scale(const ScaleTransformOperation* scale) {
    if (scale)
      Init(scale->X(), scale->Y(), scale->Z(), false);
    else
      Init(1, 1, 1, true);
  }
  explicit Scale(const InterpolableValue& value) {
    const InterpolableList& list = ToInterpolableList(value);
    if (list.length() == 0) {
      Init(1, 1, 1, true);
      return;
    }
    Init(ToInterpolableNumber(*list.Get(0)).Value(),
         ToInterpolableNumber(*list.Get(1)).Value(),
         ToInterpolableNumber(*list.Get(2)).Value(), false);
  }

  void Init(double x, double y, double z, bool is_value_none) {
    array[0] = x;
    array[1] = y;
    array[2] = z;
    is_none = is_value_none;
  }

  InterpolationValue CreateInterpolationValue() const;

  bool operator==(const Scale& other) const {
    for (size_t i = 0; i < 3; i++) {
      if (array[i] != other.array[i])
        return false;
    }
    return is_none == other.is_none;
  }

  double array[3];
  bool is_none;
};

std::unique_ptr<InterpolableValue> CreateScaleIdentity() {
  auto list = std::make_unique<InterpolableList>(3);
  for (wtf_size_t i = 0; i < 3; i++)
    list->Set(i, std::make_unique<InterpolableNumber>(1));
  return std::move(list);
}

class InheritedScaleChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedScaleChecker(const Scale& scale) : scale_(scale) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return scale_ == Scale(state.ParentStyle()->Scale());
  }

  const Scale scale_;
};

}  // namespace

class CSSScaleNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSScaleNonInterpolableValue() final = default;

  static scoped_refptr<CSSScaleNonInterpolableValue> Create(
      const Scale& scale) {
    return base::AdoptRef(
        new CSSScaleNonInterpolableValue(scale, scale, false, false));
  }

  static scoped_refptr<CSSScaleNonInterpolableValue> CreateAdditive(
      const CSSScaleNonInterpolableValue& other) {
    const bool is_additive = true;
    return base::AdoptRef(new CSSScaleNonInterpolableValue(
        other.start_, other.end_, is_additive, is_additive));
  }

  static scoped_refptr<CSSScaleNonInterpolableValue> Merge(
      const CSSScaleNonInterpolableValue& start,
      const CSSScaleNonInterpolableValue& end) {
    return base::AdoptRef(new CSSScaleNonInterpolableValue(
        start.Start(), end.end(), start.IsStartAdditive(),
        end.IsEndAdditive()));
  }

  const Scale& Start() const { return start_; }
  const Scale& end() const { return end_; }
  bool IsStartAdditive() const { return is_start_additive_; }
  bool IsEndAdditive() const { return is_end_additive_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSScaleNonInterpolableValue(const Scale& start,
                               const Scale& end,
                               bool is_start_additive,
                               bool is_end_additive)
      : start_(start),
        end_(end),
        is_start_additive_(is_start_additive),
        is_end_additive_(is_end_additive) {}

  const Scale start_;
  const Scale end_;
  bool is_start_additive_;
  bool is_end_additive_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSScaleNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSScaleNonInterpolableValue);

InterpolationValue Scale::CreateInterpolationValue() const {
  if (is_none) {
    return InterpolationValue(std::make_unique<InterpolableList>(0),
                              CSSScaleNonInterpolableValue::Create(*this));
  }

  auto list = std::make_unique<InterpolableList>(3);
  for (wtf_size_t i = 0; i < 3; i++)
    list->Set(i, std::make_unique<InterpolableNumber>(array[i]));
  return InterpolationValue(std::move(list),
                            CSSScaleNonInterpolableValue::Create(*this));
}

InterpolationValue CSSScaleInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return Scale(1, 1, 1).CreateInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return Scale().CreateInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  Scale inherited_scale(state.ParentStyle()->Scale());
  conversion_checkers.push_back(
      std::make_unique<InheritedScaleChecker>(inherited_scale));
  return inherited_scale.CreateInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList())
    return Scale().CreateInterpolationValue();

  const auto& list = To<CSSValueList>(value);
  DCHECK(list.length() >= 1 && list.length() <= 3);

  if (list.length() == 1) {
    double scale = To<CSSPrimitiveValue>(list.Item(0)).GetDoubleValue();
    // single value defines a 2d scale according to the spec
    // see https://drafts.csswg.org/css-transforms-2/#propdef-scale
    return Scale(scale, scale, 1).CreateInterpolationValue();
  } else if (list.length() == 2) {
    double x_scale = To<CSSPrimitiveValue>(list.Item(0)).GetDoubleValue();
    double y_scale = To<CSSPrimitiveValue>(list.Item(1)).GetDoubleValue();
    return Scale(x_scale, y_scale, 1).CreateInterpolationValue();
  } else {
    double x_scale = To<CSSPrimitiveValue>(list.Item(0)).GetDoubleValue();
    double y_scale = To<CSSPrimitiveValue>(list.Item(1)).GetDoubleValue();
    double z_scale = To<CSSPrimitiveValue>(list.Item(2)).GetDoubleValue();
    return Scale(x_scale, y_scale, z_scale).CreateInterpolationValue();
  }
}

InterpolationValue CSSScaleInterpolationType::PreInterpolationCompositeIfNeeded(
    InterpolationValue value,
    const InterpolationValue& underlying,
    EffectModel::CompositeOperation,
    ConversionCheckers&) const {
  value.non_interpolable_value = CSSScaleNonInterpolableValue::CreateAdditive(
      ToCSSScaleNonInterpolableValue(*value.non_interpolable_value));
  return value;
}

PairwiseInterpolationValue CSSScaleInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  wtf_size_t start_list_length =
      ToInterpolableList(*start.interpolable_value).length();
  wtf_size_t end_list_length =
      ToInterpolableList(*end.interpolable_value).length();
  if (start_list_length < end_list_length)
    start.interpolable_value = CreateScaleIdentity();
  else if (end_list_length < start_list_length)
    end.interpolable_value = CreateScaleIdentity();

  return PairwiseInterpolationValue(
      std::move(start.interpolable_value), std::move(end.interpolable_value),
      CSSScaleNonInterpolableValue::Merge(
          ToCSSScaleNonInterpolableValue(*start.non_interpolable_value),
          ToCSSScaleNonInterpolableValue(*end.non_interpolable_value)));
}

InterpolationValue
CSSScaleInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return Scale(style.Scale()).CreateInterpolationValue();
}

void CSSScaleInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (ToInterpolableList(
          *underlying_value_owner.MutableValue().interpolable_value)
          .length() == 0) {
    underlying_value_owner.MutableValue().interpolable_value =
        CreateScaleIdentity();
  }

  const CSSScaleNonInterpolableValue& metadata =
      ToCSSScaleNonInterpolableValue(*value.non_interpolable_value);
  DCHECK(metadata.IsStartAdditive() || metadata.IsEndAdditive());

  InterpolableList& underlying_list = ToInterpolableList(
      *underlying_value_owner.MutableValue().interpolable_value);
  for (wtf_size_t i = 0; i < 3; i++) {
    InterpolableNumber& underlying =
        ToInterpolableNumber(*underlying_list.GetMutable(i));

    double start = metadata.Start().array[i] *
                   (metadata.IsStartAdditive() ? underlying.Value() : 1);
    double end = metadata.end().array[i] *
                 (metadata.IsEndAdditive() ? underlying.Value() : 1);
    underlying.Set(Blend(start, end, interpolation_fraction));
  }
}

void CSSScaleInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  Scale scale(interpolable_value);
  if (scale.is_none) {
    state.Style()->SetScale(nullptr);
    return;
  }
  state.Style()->SetScale(ScaleTransformOperation::Create(
      scale.array[0], scale.array[1], scale.array[2],
      TransformOperation::kScale3D));
}

}  // namespace blink
