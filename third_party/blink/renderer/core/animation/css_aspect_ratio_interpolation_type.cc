// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_aspect_ratio_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_aspect_ratio.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_aspect_ratio.h"

namespace blink {

class CSSAspectRatioNonInterpolableValue final : public NonInterpolableValue {
 public:
  ~CSSAspectRatioNonInterpolableValue() final = default;

  static scoped_refptr<CSSAspectRatioNonInterpolableValue> Create(
      StyleAspectRatio aspect_ratio) {
    return base::AdoptRef(
        new CSSAspectRatioNonInterpolableValue(aspect_ratio.GetType()));
  }

  EAspectRatioType GetAspectRatioType() const { return type_; }

  bool IsCompatibleWith(const CSSAspectRatioNonInterpolableValue& other) const {
    if (GetAspectRatioType() == EAspectRatioType::kAuto ||
        GetAspectRatioType() != other.GetAspectRatioType())
      return false;
    return true;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  explicit CSSAspectRatioNonInterpolableValue(EAspectRatioType type)
      : type_(type) {}

  EAspectRatioType type_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSAspectRatioNonInterpolableValue);
template <>
struct DowncastTraits<CSSAspectRatioNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSAspectRatioNonInterpolableValue::static_type_;
  }
};

class InheritedAspectRatioChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedAspectRatioChecker(StyleAspectRatio aspect_ratio)
      : aspect_ratio_(aspect_ratio) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return state.ParentStyle()->AspectRatio() == aspect_ratio_;
  }

  const StyleAspectRatio aspect_ratio_;
};

InterpolableValue*
CSSAspectRatioInterpolationType::CreateInterpolableAspectRatio(
    const StyleAspectRatio& aspect_ratio) {
  return InterpolableAspectRatio::MaybeCreate(aspect_ratio);
}

PairwiseInterpolationValue CSSAspectRatioInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (!To<CSSAspectRatioNonInterpolableValue>(*start.non_interpolable_value)
           .IsCompatibleWith(To<CSSAspectRatioNonInterpolableValue>(
               *end.non_interpolable_value))) {
    return nullptr;
  }
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue CSSAspectRatioInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSAspectRatioInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  StyleAspectRatio initial_ratio =
      state.GetDocument().GetStyleResolver().InitialStyle().AspectRatio();
  return InterpolationValue(
      CreateInterpolableAspectRatio(initial_ratio),
      CSSAspectRatioNonInterpolableValue::Create(initial_ratio));
}

InterpolationValue CSSAspectRatioInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;

  StyleAspectRatio inherited_aspect_ratio = state.ParentStyle()->AspectRatio();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedAspectRatioChecker>(
          inherited_aspect_ratio));
  if (inherited_aspect_ratio.IsAuto())
    return nullptr;

  return InterpolationValue(
      CreateInterpolableAspectRatio(inherited_aspect_ratio),
      CSSAspectRatioNonInterpolableValue::Create(inherited_aspect_ratio));
}

InterpolationValue
CSSAspectRatioInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return InterpolationValue(
      CreateInterpolableAspectRatio(style.AspectRatio()),
      CSSAspectRatioNonInterpolableValue::Create(style.AspectRatio()));
}

InterpolationValue CSSAspectRatioInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers&) const {
  StyleAspectRatio ratio =
      StyleBuilderConverter::ConvertAspectRatio(*state, value);
  return InterpolationValue(CreateInterpolableAspectRatio(ratio),
                            CSSAspectRatioNonInterpolableValue::Create(ratio));
}

void CSSAspectRatioInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const auto& aspect_ratio = To<InterpolableAspectRatio>(interpolable_value);
  state.StyleBuilder().SetAspectRatio(StyleAspectRatio(
      To<CSSAspectRatioNonInterpolableValue>(non_interpolable_value)
          ->GetAspectRatioType(),
      aspect_ratio.GetRatio()));
}
void CSSAspectRatioInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

}  // namespace blink
