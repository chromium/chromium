// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_display_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSDisplayNonInterpolableValue final : public NonInterpolableValue {
 public:
  ~CSSDisplayNonInterpolableValue() final = default;

  static scoped_refptr<CSSDisplayNonInterpolableValue> Create(EDisplay start,
                                                              EDisplay end) {
    return base::AdoptRef(new CSSDisplayNonInterpolableValue(start, end));
  }

  EDisplay Display() const {
    DCHECK_EQ(start_, end_);
    return start_;
  }

  EDisplay Display(double fraction) const {
    if ((start_ == EDisplay::kNone || end_ == EDisplay::kNone) &&
        start_ != end_) {
      // No halfway transition when transitioning to or from display:none
      if (start_ == EDisplay::kNone) {
        return fraction > 0 ? end_ : start_;
      } else {
        return fraction >= 1 ? end_ : start_;
      }
    }
    return fraction >= 0.5 ? end_ : start_;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSDisplayNonInterpolableValue(EDisplay start, EDisplay end)
      : start_(start), end_(end) {}

  const EDisplay start_;
  const EDisplay end_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSDisplayNonInterpolableValue);
template <>
struct DowncastTraits<CSSDisplayNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSDisplayNonInterpolableValue::static_type_;
  }
};

class UnderlyingDisplayChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingDisplayChecker(EDisplay display) : display_(display) {}

  ~UnderlyingDisplayChecker() final = default;

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    double underlying_fraction =
        To<InterpolableNumber>(*underlying.interpolable_value)
            .Value(state.CssToLengthConversionData());
    EDisplay underlying_display =
        To<CSSDisplayNonInterpolableValue>(*underlying.non_interpolable_value)
            .Display(underlying_fraction);
    return display_ == underlying_display;
  }

  const EDisplay display_;
};

class InheritedDisplayChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedDisplayChecker(EDisplay display) : display_(display) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return display_ == state.ParentStyle()->Display();
  }

  const EDisplay display_;
};

InterpolationValue CSSDisplayInterpolationType::CreateDisplayValue(
    EDisplay display) const {
  return InterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(0),
      CSSDisplayNonInterpolableValue::Create(display, display));
}

InterpolationValue CSSDisplayInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  double underlying_fraction =
      To<InterpolableNumber>(*underlying.interpolable_value)
          .Value(CSSToLengthConversionData());
  EDisplay underlying_display =
      To<CSSDisplayNonInterpolableValue>(*underlying.non_interpolable_value)
          .Display(underlying_fraction);
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingDisplayChecker>(underlying_display));
  return CreateDisplayValue(underlying_display);
}

InterpolationValue CSSDisplayInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers&) const {
  return CreateDisplayValue(
      state.GetDocument().GetStyleResolver().InitialStyle().Display());
}

InterpolationValue CSSDisplayInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }
  EDisplay inherited_display = state.ParentStyle()->Display();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedDisplayChecker>(inherited_display));
  return CreateDisplayValue(inherited_display);
}

InterpolationValue CSSDisplayInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversion_checkers) const {
  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return nullptr;
  }

  CSSValueID keyword = identifier_value->GetValueID();

  switch (keyword) {
    case CSSValueID::kBlock:
    case CSSValueID::kContents:
    case CSSValueID::kFlex:
    case CSSValueID::kFlowRoot:
    case CSSValueID::kGrid:
    case CSSValueID::kInline:
    case CSSValueID::kInlineBlock:
    case CSSValueID::kInlineFlex:
    case CSSValueID::kInlineGrid:
    case CSSValueID::kListItem:
    case CSSValueID::kNone:
    case CSSValueID::kTable:
    case CSSValueID::kTableRow:
      return CreateDisplayValue(identifier_value->ConvertTo<EDisplay>());
    default:
      return nullptr;
  }
}

InterpolationValue
CSSDisplayInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateDisplayValue(style.Display());
}

PairwiseInterpolationValue CSSDisplayInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  EDisplay start_display =
      To<CSSDisplayNonInterpolableValue>(*start.non_interpolable_value)
          .Display();
  EDisplay end_display =
      To<CSSDisplayNonInterpolableValue>(*end.non_interpolable_value).Display();
  return PairwiseInterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(0),
      MakeGarbageCollected<InterpolableNumber>(1),
      CSSDisplayNonInterpolableValue::Create(start_display, end_display));
}

void CSSDisplayInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(*this, value);
}

void CSSDisplayInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  // Display interpolation has been deferred to application time here due to
  // its non-linear behaviour.
  double fraction = To<InterpolableNumber>(interpolable_value)
                        .Value(state.CssToLengthConversionData());
  EDisplay display = To<CSSDisplayNonInterpolableValue>(non_interpolable_value)
                         ->Display(fraction);
  state.StyleBuilder().SetDisplay(display);
}

}  // namespace blink
