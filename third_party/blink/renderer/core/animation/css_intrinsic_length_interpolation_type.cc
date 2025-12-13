// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_intrinsic_length_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_intrinsic_length.h"

namespace blink {

class CSSIntrinsicLengthNonInterpolableValue final
    : public NonInterpolableValue {
 public:
  enum EType { kNone, kAutoAndLength, kLength, kAutoAndNone };

  explicit CSSIntrinsicLengthNonInterpolableValue(EType type) : type_(type) {}
  ~CSSIntrinsicLengthNonInterpolableValue() final = default;

  static CSSIntrinsicLengthNonInterpolableValue* Create(
      const StyleIntrinsicLength& intrinsic_dimension) {
    EType type = kNone;
    if (intrinsic_dimension.HasAuto() &&
        intrinsic_dimension.GetLength().has_value()) {
      type = kAutoAndLength;
    } else if (intrinsic_dimension.HasAuto()) {
      type = kAutoAndNone;
    } else if (intrinsic_dimension.GetLength().has_value()) {
      type = kLength;
    }
    return MakeGarbageCollected<CSSIntrinsicLengthNonInterpolableValue>(type);
  }

  bool HasNone() const { return type_ == kNone || type_ == kAutoAndNone; }
  bool HasAuto() const {
    return type_ == kAutoAndLength || type_ == kAutoAndNone;
  }

  bool IsCompatibleWith(
      const CSSIntrinsicLengthNonInterpolableValue& other) const {
    if (HasNone() || other.HasNone() || (HasAuto() != other.HasAuto())) {
      return false;
    }
    return true;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  EType type_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSIntrinsicLengthNonInterpolableValue);
template <>
struct DowncastTraits<CSSIntrinsicLengthNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() ==
           CSSIntrinsicLengthNonInterpolableValue::static_type_;
  }
};

class InheritedIntrinsicDimensionChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedIntrinsicDimensionChecker(
      bool is_width,
      const StyleIntrinsicLength& intrinsic_dimension)
      : is_width_(is_width), intrinsic_dimension_(intrinsic_dimension) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    if (is_width_) {
      return state.ParentStyle()->ContainIntrinsicWidth() ==
             intrinsic_dimension_;
    }
    return state.ParentStyle()->ContainIntrinsicHeight() ==
           intrinsic_dimension_;
  }

  bool is_width_;
  const StyleIntrinsicLength intrinsic_dimension_;
};

InterpolableValue*
CSSIntrinsicLengthInterpolationType::CreateInterpolableIntrinsicDimension(
    const StyleIntrinsicLength& intrinsic_dimension,
    float zoom) {
  const auto& length = intrinsic_dimension.GetLength();
  if (!length) {
    return nullptr;
  }

  DCHECK(length->IsFixed());
  CHECK_GT(zoom, 0.f);
  return InterpolableLength::CreatePixels(length->Pixels() / zoom);
}

PairwiseInterpolationValue
CSSIntrinsicLengthInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (!To<CSSIntrinsicLengthNonInterpolableValue>(*start.non_interpolable_value)
           .IsCompatibleWith(To<CSSIntrinsicLengthNonInterpolableValue>(
               *end.non_interpolable_value))) {
    return nullptr;
  }
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

StyleIntrinsicLength CSSIntrinsicLengthInterpolationType::GetIntrinsicDimension(
    const ComputedStyle& style) const {
  return CssProperty().PropertyID() == CSSPropertyID::kContainIntrinsicWidth
             ? style.ContainIntrinsicWidth()
             : style.ContainIntrinsicHeight();
}

void CSSIntrinsicLengthInterpolationType::SetIntrinsicDimension(
    ComputedStyleBuilder& builder,
    const StyleIntrinsicLength& dimension) const {
  if (CssProperty().PropertyID() == CSSPropertyID::kContainIntrinsicWidth)
    builder.SetContainIntrinsicWidth(dimension);
  else
    builder.SetContainIntrinsicHeight(dimension);
}

InterpolationValue CSSIntrinsicLengthInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSIntrinsicLengthInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  StyleIntrinsicLength initial_dimension = GetIntrinsicDimension(
      state.GetDocument().GetStyleResolver().InitialStyle());
  return InterpolationValue(
      CreateInterpolableIntrinsicDimension(initial_dimension, 1.f),
      CSSIntrinsicLengthNonInterpolableValue::Create(initial_dimension));
}

InterpolationValue CSSIntrinsicLengthInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;

  StyleIntrinsicLength inherited_intrinsic_dimension =
      GetIntrinsicDimension(*state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedIntrinsicDimensionChecker>(
          CssProperty().PropertyID() == CSSPropertyID::kContainIntrinsicWidth,
          inherited_intrinsic_dimension));
  if (inherited_intrinsic_dimension.IsNoOp()) {
    return nullptr;
  }

  return InterpolationValue(
      CreateInterpolableIntrinsicDimension(
          inherited_intrinsic_dimension, state.ParentStyle()->EffectiveZoom()),
      CSSIntrinsicLengthNonInterpolableValue::Create(
          inherited_intrinsic_dimension));
}

InterpolationValue CSSIntrinsicLengthInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  StyleIntrinsicLength dimension = GetIntrinsicDimension(style);
  return InterpolationValue(
      CreateInterpolableIntrinsicDimension(dimension, style.EffectiveZoom()),
      CSSIntrinsicLengthNonInterpolableValue::Create(dimension));
}

InterpolationValue CSSIntrinsicLengthInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers&) const {
  const StyleIntrinsicLength& dimension =
      StyleBuilderConverter::ConvertIntrinsicDimension(state, value);
  return InterpolationValue(
      CreateInterpolableIntrinsicDimension(
          dimension, state.StyleBuilder().EffectiveZoom()),
      CSSIntrinsicLengthNonInterpolableValue::Create(dimension));
}

void CSSIntrinsicLengthInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const auto& interpolable = To<InterpolableLength>(interpolable_value);
  const auto* non_interpolable =
      To<CSSIntrinsicLengthNonInterpolableValue>(non_interpolable_value);
  if (non_interpolable->HasNone()) {
    SetIntrinsicDimension(
        state.StyleBuilder(),
        StyleIntrinsicLength(std::nullopt,
                             {.has_auto = non_interpolable->HasAuto()}));
  } else {
    SetIntrinsicDimension(
        state.StyleBuilder(),
        StyleIntrinsicLength(
            interpolable.CreateLength(state.CssToLengthConversionData(),
                                      Length::ValueRange::kNonNegative),
            {.has_auto = non_interpolable->HasAuto()}));
  }
}
void CSSIntrinsicLengthInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

}  // namespace blink
