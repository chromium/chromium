// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_offset_rotate_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_offset_rotation.h"

namespace blink {

class CSSOffsetRotationNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSOffsetRotationNonInterpolableValue() override = default;

  static scoped_refptr<CSSOffsetRotationNonInterpolableValue> Create(
      OffsetRotationType rotation_type) {
    return base::AdoptRef(
        new CSSOffsetRotationNonInterpolableValue(rotation_type));
  }

  OffsetRotationType RotationType() const { return rotation_type_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSOffsetRotationNonInterpolableValue(OffsetRotationType rotation_type)
      : rotation_type_(rotation_type) {}

  OffsetRotationType rotation_type_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSOffsetRotationNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSOffsetRotationNonInterpolableValue);

namespace {

class UnderlyingRotationTypeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingRotationTypeChecker(
      OffsetRotationType underlying_rotation_type)
      : underlying_rotation_type_(underlying_rotation_type) {}

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return underlying_rotation_type_ == ToCSSOffsetRotationNonInterpolableValue(
                                            *underlying.non_interpolable_value)
                                            .RotationType();
  }

 private:
  OffsetRotationType underlying_rotation_type_;
};

class InheritedOffsetRotationChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedOffsetRotationChecker(
      StyleOffsetRotation inherited_rotation)
      : inherited_rotation_(inherited_rotation) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return inherited_rotation_ == state.ParentStyle()->OffsetRotate();
  }

 private:
  StyleOffsetRotation inherited_rotation_;
};

InterpolationValue ConvertOffsetRotate(const StyleOffsetRotation& rotation) {
  return InterpolationValue(
      std::make_unique<InterpolableNumber>(rotation.angle),
      CSSOffsetRotationNonInterpolableValue::Create(rotation.type));
}

}  // namespace

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  OffsetRotationType underlying_rotation_type =
      ToCSSOffsetRotationNonInterpolableValue(
          *underlying.non_interpolable_value)
          .RotationType();
  conversion_checkers.push_back(std::make_unique<UnderlyingRotationTypeChecker>(
      underlying_rotation_type));
  return ConvertOffsetRotate(StyleOffsetRotation(0, underlying_rotation_type));
}

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return ConvertOffsetRotate(StyleOffsetRotation(0, OffsetRotationType::kAuto));
}

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const StyleOffsetRotation& inherited_rotation =
      state.ParentStyle()->OffsetRotate();
  conversion_checkers.push_back(
      std::make_unique<InheritedOffsetRotationChecker>(inherited_rotation));
  return ConvertOffsetRotate(inherited_rotation);
}

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  return ConvertOffsetRotate(StyleBuilderConverter::ConvertOffsetRotate(value));
}

PairwiseInterpolationValue CSSOffsetRotateInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const OffsetRotationType& start_type =
      ToCSSOffsetRotationNonInterpolableValue(*start.non_interpolable_value)
          .RotationType();
  const OffsetRotationType& end_type =
      ToCSSOffsetRotationNonInterpolableValue(*end.non_interpolable_value)
          .RotationType();
  if (start_type != end_type)
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue
CSSOffsetRotateInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertOffsetRotate(style.OffsetRotate());
}

void CSSOffsetRotateInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const OffsetRotationType& underlying_type =
      ToCSSOffsetRotationNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value)
          .RotationType();
  const OffsetRotationType& rotation_type =
      ToCSSOffsetRotationNonInterpolableValue(*value.non_interpolable_value)
          .RotationType();
  if (underlying_type == rotation_type) {
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  } else {
    underlying_value_owner.Set(*this, value);
  }
}

void CSSOffsetRotateInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  state.Style()->SetOffsetRotate(StyleOffsetRotation(
      clampTo<float>(ToInterpolableNumber(interpolable_value).Value()),
      ToCSSOffsetRotationNonInterpolableValue(*non_interpolable_value)
          .RotationType()));
}

}  // namespace blink
