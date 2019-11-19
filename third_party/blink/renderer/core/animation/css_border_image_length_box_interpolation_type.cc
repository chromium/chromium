// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_border_image_length_box_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/side_index.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

enum class SideType {
  kNumber,
  kAuto,
  kLength,
};

const BorderImageLengthBox& GetBorderImageLengthBox(
    const CSSProperty& property,
    const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBorderImageOutset:
      return style.BorderImageOutset();
    case CSSPropertyID::kBorderImageWidth:
      return style.BorderImageWidth();
    case CSSPropertyID::kWebkitMaskBoxImageOutset:
      return style.MaskBoxImageOutset();
    case CSSPropertyID::kWebkitMaskBoxImageWidth:
      return style.MaskBoxImageWidth();
    default:
      NOTREACHED();
      return GetBorderImageLengthBox(
          CSSProperty::Get(CSSPropertyID::kBorderImageOutset),
          ComputedStyle::InitialStyle());
  }
}

void SetBorderImageLengthBox(const CSSProperty& property,
                             ComputedStyle& style,
                             const BorderImageLengthBox& box) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBorderImageOutset:
      style.SetBorderImageOutset(box);
      break;
    case CSSPropertyID::kWebkitMaskBoxImageOutset:
      style.SetMaskBoxImageOutset(box);
      break;
    case CSSPropertyID::kBorderImageWidth:
      style.SetBorderImageWidth(box);
      break;
    case CSSPropertyID::kWebkitMaskBoxImageWidth:
      style.SetMaskBoxImageWidth(box);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

// The NonInterpolableValue for the CSSBorderImageLengthBoxInterpolationType
// as a whole is a NonInterpolableList with kSideIndexCount items. Each entry
// in that list is either an instance of this class, or it's the
// NonInterpolableValue returned by LengthInterpolationFunctions.
class CSSBorderImageLengthBoxSideNonInterpolableValue
    : public NonInterpolableValue {
 public:
  static scoped_refptr<CSSBorderImageLengthBoxSideNonInterpolableValue> Create(
      SideType side_type) {
    DCHECK_NE(SideType::kLength, side_type);
    return base::AdoptRef(
        new CSSBorderImageLengthBoxSideNonInterpolableValue(side_type));
  }

  SideType GetSideType() const { return side_type_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSBorderImageLengthBoxSideNonInterpolableValue(const SideType side_type)
      : side_type_(side_type) {}

  const SideType side_type_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(
    CSSBorderImageLengthBoxSideNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(
    CSSBorderImageLengthBoxSideNonInterpolableValue);

namespace {

SideType GetSideType(const BorderImageLength& side) {
  if (side.IsNumber()) {
    return SideType::kNumber;
  }
  if (side.length().IsAuto()) {
    return SideType::kAuto;
  }
  DCHECK(side.length().IsSpecified());
  return SideType::kLength;
}

SideType GetSideType(const CSSValue& side) {
  auto* side_primitive_value = DynamicTo<CSSPrimitiveValue>(side);
  if (side_primitive_value && side_primitive_value->IsNumber()) {
    return SideType::kNumber;
  }
  auto* side_identifier_value = DynamicTo<CSSIdentifierValue>(side);
  if (side_identifier_value &&
      side_identifier_value->GetValueID() == CSSValueID::kAuto) {
    return SideType::kAuto;
  }
  return SideType::kLength;
}

SideType GetSideType(const NonInterpolableValue* side) {
  // We interpret nullptr as kLength, because LengthInterpolationFunctions
  // returns a nullptr NonInterpolableValue if there is no percent unit.
  //
  // In cases where LengthInterpolationFunctions is not used to convert the
  // value (kAuto, kNumber), we will always have a non-interpolable value of
  // type CSSBorderImageLengthBoxSideNonInterpolableValue.
  if (!side || !IsCSSBorderImageLengthBoxSideNonInterpolableValue(side))
    return SideType::kLength;
  return ToCSSBorderImageLengthBoxSideNonInterpolableValue(*side).GetSideType();
}

struct SideTypes {
  explicit SideTypes(const BorderImageLengthBox& box) {
    type[kSideTop] = GetSideType(box.Top());
    type[kSideRight] = GetSideType(box.Right());
    type[kSideBottom] = GetSideType(box.Bottom());
    type[kSideLeft] = GetSideType(box.Left());
  }
  explicit SideTypes(const CSSQuadValue& quad) {
    type[kSideTop] = GetSideType(*quad.Top());
    type[kSideRight] = GetSideType(*quad.Right());
    type[kSideBottom] = GetSideType(*quad.Bottom());
    type[kSideLeft] = GetSideType(*quad.Left());
  }
  explicit SideTypes(const InterpolationValue& underlying) {
    const auto& non_interpolable_list =
        ToNonInterpolableList(*underlying.non_interpolable_value);
    DCHECK_EQ(kSideIndexCount, non_interpolable_list.length());
    type[kSideTop] = GetSideType(non_interpolable_list.Get(0));
    type[kSideRight] = GetSideType(non_interpolable_list.Get(1));
    type[kSideBottom] = GetSideType(non_interpolable_list.Get(2));
    type[kSideLeft] = GetSideType(non_interpolable_list.Get(3));
  }

  bool operator==(const SideTypes& other) const {
    for (size_t i = 0; i < kSideIndexCount; i++) {
      if (type[i] != other.type[i])
        return false;
    }
    return true;
  }
  bool operator!=(const SideTypes& other) const { return !(*this == other); }

  SideType type[kSideIndexCount];
};

class UnderlyingSideTypesChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:

  explicit UnderlyingSideTypesChecker(const SideTypes& underlying_side_types)
      : underlying_side_types_(underlying_side_types) {}

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return underlying_side_types_ == SideTypes(underlying);
  }

  const SideTypes underlying_side_types_;
};

class InheritedSideTypesChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedSideTypesChecker(const CSSProperty& property,
                            const SideTypes& inherited_side_types)
      : property_(property), inherited_side_types_(inherited_side_types) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return inherited_side_types_ ==
           SideTypes(GetBorderImageLengthBox(property_, *state.ParentStyle()));
  }

  const CSSProperty& property_;
  const SideTypes inherited_side_types_;
};

InterpolationValue ConvertBorderImageNumberSide(double number) {
  return InterpolationValue(
      std::make_unique<InterpolableNumber>(number),
      CSSBorderImageLengthBoxSideNonInterpolableValue::Create(
          SideType::kNumber));
}

InterpolationValue ConvertBorderImageAutoSide() {
  return InterpolationValue(
      std::make_unique<InterpolableList>(0),
      CSSBorderImageLengthBoxSideNonInterpolableValue::Create(SideType::kAuto));
}

InterpolationValue ConvertBorderImageLengthBox(const BorderImageLengthBox& box,
                                               double zoom) {
  auto list = std::make_unique<InterpolableList>(kSideIndexCount);
  Vector<scoped_refptr<const NonInterpolableValue>> non_interpolable_values(
      kSideIndexCount);
  const BorderImageLength* sides[kSideIndexCount] = {};
  sides[kSideTop] = &box.Top();
  sides[kSideRight] = &box.Right();
  sides[kSideBottom] = &box.Bottom();
  sides[kSideLeft] = &box.Left();

  return ListInterpolationFunctions::CreateList(
      kSideIndexCount, [&sides, zoom](wtf_size_t index) {
        const BorderImageLength& side = *sides[index];
        if (side.IsNumber())
          return ConvertBorderImageNumberSide(side.Number());
        if (side.length().IsAuto())
          return ConvertBorderImageAutoSide();
        return InterpolationValue(
            InterpolableLength::MaybeConvertLength(side.length(), zoom));
      });
}

void CompositeSide(UnderlyingValue& underlying_value,
                   double underlying_fraction,
                   const InterpolableValue& interpolable_value,
                   const NonInterpolableValue* non_interpolable_value) {
  switch (GetSideType(non_interpolable_value)) {
    case SideType::kNumber:
    case SideType::kLength:
      underlying_value.MutableInterpolableValue().ScaleAndAdd(
          underlying_fraction, interpolable_value);
      break;
    case SideType::kAuto:
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool NonInterpolableSidesAreCompatible(const NonInterpolableValue* a,
                                       const NonInterpolableValue* b) {
  return GetSideType(a) == GetSideType(b);
}

}  // namespace

InterpolationValue
CSSBorderImageLengthBoxInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  SideTypes underlying_side_types(underlying);
  conversion_checkers.push_back(
      std::make_unique<UnderlyingSideTypesChecker>(underlying_side_types));
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue
CSSBorderImageLengthBoxInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return ConvertBorderImageLengthBox(
      GetBorderImageLengthBox(CssProperty(), ComputedStyle::InitialStyle()), 1);
}

InterpolationValue
CSSBorderImageLengthBoxInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const BorderImageLengthBox& inherited =
      GetBorderImageLengthBox(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(std::make_unique<InheritedSideTypesChecker>(
      CssProperty(), SideTypes(inherited)));
  return ConvertBorderImageLengthBox(inherited,
                                     state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSBorderImageLengthBoxInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  const auto* quad = DynamicTo<CSSQuadValue>(value);
  if (!quad)
    return nullptr;

  auto list = std::make_unique<InterpolableList>(kSideIndexCount);
  Vector<scoped_refptr<const NonInterpolableValue>> non_interpolable_values(
      kSideIndexCount);
  const CSSValue* sides[kSideIndexCount] = {};
  sides[kSideTop] = quad->Top();
  sides[kSideRight] = quad->Right();
  sides[kSideBottom] = quad->Bottom();
  sides[kSideLeft] = quad->Left();

  return ListInterpolationFunctions::CreateList(
      kSideIndexCount, [&sides](wtf_size_t index) {
        const CSSValue& side = *sides[index];

        auto* side_primitive_value = DynamicTo<CSSPrimitiveValue>(side);
        if (side_primitive_value && side_primitive_value->IsNumber()) {
          return ConvertBorderImageNumberSide(
              side_primitive_value->GetDoubleValue());
        }

        auto* side_identifier_value = DynamicTo<CSSIdentifierValue>(side);
        if (side_identifier_value &&
            side_identifier_value->GetValueID() == CSSValueID::kAuto) {
          return ConvertBorderImageAutoSide();
        }

        return InterpolationValue(
            InterpolableLength::MaybeConvertCSSValue(side));
      });
}

InterpolationValue CSSBorderImageLengthBoxInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  return ConvertBorderImageLengthBox(
      GetBorderImageLengthBox(CssProperty(), style), style.EffectiveZoom());
}

PairwiseInterpolationValue
CSSBorderImageLengthBoxInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (SideTypes(start) != SideTypes(end))
    return nullptr;

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

void CSSBorderImageLengthBoxInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  ListInterpolationFunctions::Composite(
      underlying_value_owner, underlying_fraction, *this, value,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(
          ListInterpolationFunctions::InterpolableValuesKnownCompatible),
      WTF::BindRepeating(NonInterpolableSidesAreCompatible),
      WTF::BindRepeating(CompositeSide));
}

void CSSBorderImageLengthBoxInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  const NonInterpolableList& non_interpolable_list =
      ToNonInterpolableList(*non_interpolable_value);
  const auto& convert_side = [&list, &non_interpolable_list,
                              &state](wtf_size_t index) -> BorderImageLength {
    switch (GetSideType(non_interpolable_list.Get(index))) {
      case SideType::kNumber:
        return clampTo<double>(ToInterpolableNumber(list.Get(index))->Value(),
                               0);
      case SideType::kAuto:
        return Length::Auto();
      case SideType::kLength:
        return To<InterpolableLength>(*list.Get(index))
            .CreateLength(state.CssToLengthConversionData(),
                          kValueRangeNonNegative);
      default:
        NOTREACHED();
        return Length::Auto();
    }
  };
  BorderImageLengthBox box(convert_side(kSideTop), convert_side(kSideRight),
                           convert_side(kSideBottom), convert_side(kSideLeft));
  SetBorderImageLengthBox(CssProperty(), *state.Style(), box);
}

}  // namespace blink
