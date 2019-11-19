// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_visibility_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSVisibilityNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSVisibilityNonInterpolableValue() final = default;

  static scoped_refptr<CSSVisibilityNonInterpolableValue> Create(
      EVisibility start,
      EVisibility end) {
    return base::AdoptRef(new CSSVisibilityNonInterpolableValue(start, end));
  }

  EVisibility Visibility() const {
    DCHECK(is_single_);
    return start_;
  }

  EVisibility Visibility(double fraction) const {
    if (is_single_ || fraction <= 0)
      return start_;
    if (fraction >= 1)
      return end_;
    DCHECK(start_ == EVisibility::kVisible || end_ == EVisibility::kVisible);
    return EVisibility::kVisible;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSVisibilityNonInterpolableValue(EVisibility start, EVisibility end)
      : start_(start), end_(end), is_single_(start_ == end_) {}

  const EVisibility start_;
  const EVisibility end_;
  const bool is_single_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSVisibilityNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSVisibilityNonInterpolableValue);

class UnderlyingVisibilityChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingVisibilityChecker(EVisibility visibility)
      : visibility_(visibility) {}

  ~UnderlyingVisibilityChecker() final = default;


 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    double underlying_fraction =
        ToInterpolableNumber(*underlying.interpolable_value).Value();
    EVisibility underlying_visibility =
        ToCSSVisibilityNonInterpolableValue(*underlying.non_interpolable_value)
            .Visibility(underlying_fraction);
    return visibility_ == underlying_visibility;
  }

  const EVisibility visibility_;
};

class InheritedVisibilityChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedVisibilityChecker(EVisibility visibility)
      : visibility_(visibility) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return visibility_ == state.ParentStyle()->Visibility();
  }

  const EVisibility visibility_;
};

InterpolationValue CSSVisibilityInterpolationType::CreateVisibilityValue(
    EVisibility visibility) const {
  return InterpolationValue(
      std::make_unique<InterpolableNumber>(0),
      CSSVisibilityNonInterpolableValue::Create(visibility, visibility));
}

InterpolationValue CSSVisibilityInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  double underlying_fraction =
      ToInterpolableNumber(*underlying.interpolable_value).Value();
  EVisibility underlying_visibility =
      ToCSSVisibilityNonInterpolableValue(*underlying.non_interpolable_value)
          .Visibility(underlying_fraction);
  conversion_checkers.push_back(
      std::make_unique<UnderlyingVisibilityChecker>(underlying_visibility));
  return CreateVisibilityValue(underlying_visibility);
}

InterpolationValue CSSVisibilityInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return CreateVisibilityValue(EVisibility::kVisible);
}

InterpolationValue CSSVisibilityInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  EVisibility inherited_visibility = state.ParentStyle()->Visibility();
  conversion_checkers.push_back(
      std::make_unique<InheritedVisibilityChecker>(inherited_visibility));
  return CreateVisibilityValue(inherited_visibility);
}

InterpolationValue CSSVisibilityInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversion_checkers) const {
  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value)
    return nullptr;

  CSSValueID keyword = identifier_value->GetValueID();

  switch (keyword) {
    case CSSValueID::kHidden:
    case CSSValueID::kVisible:
    case CSSValueID::kCollapse:
      return CreateVisibilityValue(identifier_value->ConvertTo<EVisibility>());
    default:
      return nullptr;
  }
}

InterpolationValue
CSSVisibilityInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateVisibilityValue(style.Visibility());
}

PairwiseInterpolationValue CSSVisibilityInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  EVisibility start_visibility =
      ToCSSVisibilityNonInterpolableValue(*start.non_interpolable_value)
          .Visibility();
  EVisibility end_visibility =
      ToCSSVisibilityNonInterpolableValue(*end.non_interpolable_value)
          .Visibility();
  // One side must be "visible".
  // Spec: https://drafts.csswg.org/css-transitions/#animtype-visibility
  if (start_visibility != end_visibility &&
      start_visibility != EVisibility::kVisible &&
      end_visibility != EVisibility::kVisible) {
    return nullptr;
  }
  return PairwiseInterpolationValue(std::make_unique<InterpolableNumber>(0),
                                    std::make_unique<InterpolableNumber>(1),
                                    CSSVisibilityNonInterpolableValue::Create(
                                        start_visibility, end_visibility));
}

void CSSVisibilityInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(*this, value);
}

void CSSVisibilityInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  // Visibility interpolation has been deferred to application time here due to
  // its non-linear behaviour.
  double fraction = ToInterpolableNumber(interpolable_value).Value();
  EVisibility visibility =
      ToCSSVisibilityNonInterpolableValue(non_interpolable_value)
          ->Visibility(fraction);
  state.Style()->SetVisibility(visibility);
}

}  // namespace blink
