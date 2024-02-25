// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_offset_rotate_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
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
template <>
struct DowncastTraits<CSSOffsetRotationNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() ==
           CSSOffsetRotationNonInterpolableValue::static_type_;
  }
};

namespace {

class UnderlyingRotationTypeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingRotationTypeChecker(
      OffsetRotationType underlying_rotation_type)
      : underlying_rotation_type_(underlying_rotation_type) {}

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return underlying_rotation_type_ ==
           To<CSSOffsetRotationNonInterpolableValue>(
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
      MakeGarbageCollected<InterpolableNumber>(
          rotation.angle, CSSPrimitiveValue::UnitType::kDegrees),
      CSSOffsetRotationNonInterpolableValue::Create(rotation.type));
}

}  // namespace

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  OffsetRotationType underlying_rotation_type =
      To<CSSOffsetRotationNonInterpolableValue>(
          *underlying.non_interpolable_value)
          .RotationType();
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingRotationTypeChecker>(
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
      MakeGarbageCollected<InheritedOffsetRotationChecker>(inherited_rotation));
  return ConvertOffsetRotate(inherited_rotation);
}

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (auto* identifier = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier->GetValueID(), CSSValueID::kAuto);
    return ConvertOffsetRotate({0.0, OffsetRotationType::kAuto});
  }

  using CSSPrimitiveValue::UnitType::kDegrees;
  CSSMathExpressionNode* angle =
      CSSMathExpressionNumericLiteral::Create(0.0, kDegrees);
  OffsetRotationType type = OffsetRotationType::kFixed;
  const auto& list = To<CSSValueList>(value);
  DCHECK(list.length() == 1 || list.length() == 2);
  for (const auto& item : list) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(item.Get());
    if (identifier_value &&
        identifier_value->GetValueID() == CSSValueID::kAuto) {
      type = OffsetRotationType::kAuto;
    } else if (identifier_value &&
               identifier_value->GetValueID() == CSSValueID::kReverse) {
      type = OffsetRotationType::kAuto;
      angle = CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
          angle, CSSMathExpressionNumericLiteral::Create(180.0, kDegrees),
          CSSMathOperator::kAdd);
    } else {
      if (const auto* numeric_value =
              DynamicTo<CSSNumericLiteralValue>(*item)) {
        angle = CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
            angle,
            CSSMathExpressionNumericLiteral::Create(
                numeric_value->ComputeDegrees(), kDegrees),
            CSSMathOperator::kAdd);
        continue;
      }
      const auto& function_value = To<CSSMathFunctionValue>(*item);
      angle = CSSMathExpressionOperation::CreateArithmeticOperation(
          angle, function_value.ExpressionNode(), CSSMathOperator::kAdd);
    }
  }
  if (const auto* numeric_literal =
          DynamicTo<CSSMathExpressionNumericLiteral>(angle)) {
    std::optional<double> degrees =
        numeric_literal->ComputeValueInCanonicalUnit();
    CHECK(degrees.has_value());
    return ConvertOffsetRotate({static_cast<float>(degrees.value()), type});
  }
  return InterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(*angle),
      CSSOffsetRotationNonInterpolableValue::Create(type));
}

PairwiseInterpolationValue CSSOffsetRotateInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const OffsetRotationType& start_type =
      To<CSSOffsetRotationNonInterpolableValue>(*start.non_interpolable_value)
          .RotationType();
  const OffsetRotationType& end_type =
      To<CSSOffsetRotationNonInterpolableValue>(*end.non_interpolable_value)
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
      To<CSSOffsetRotationNonInterpolableValue>(
          *underlying_value_owner.Value().non_interpolable_value)
          .RotationType();
  const OffsetRotationType& rotation_type =
      To<CSSOffsetRotationNonInterpolableValue>(*value.non_interpolable_value)
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
  state.StyleBuilder().SetOffsetRotate(StyleOffsetRotation(
      CSSValueClampingUtils::ClampAngle(
          To<InterpolableNumber>(interpolable_value)
              .Value(state.CssToLengthConversionData())),
      To<CSSOffsetRotationNonInterpolableValue>(*non_interpolable_value)
          .RotationType()));
}

}  // namespace blink
