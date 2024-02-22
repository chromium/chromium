// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_overlay_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSOverlayNonInterpolableValue final : public NonInterpolableValue {
 public:
  ~CSSOverlayNonInterpolableValue() final = default;

  static scoped_refptr<CSSOverlayNonInterpolableValue> Create(EOverlay start,
                                                              EOverlay end) {
    return base::AdoptRef(new CSSOverlayNonInterpolableValue(start, end));
  }

  EOverlay Overlay() const {
    DCHECK_EQ(start_, end_);
    return start_;
  }

  EOverlay Overlay(double fraction) const {
    if ((start_ == EOverlay::kNone || end_ == EOverlay::kNone) &&
        start_ != end_) {
      // No halfway transition when transitioning to or from overlay:none
      if (start_ == EOverlay::kNone) {
        return fraction > 0 ? end_ : start_;
      } else {
        return fraction >= 1 ? end_ : start_;
      }
    }
    return fraction >= 0.5 ? end_ : start_;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSOverlayNonInterpolableValue(EOverlay start, EOverlay end)
      : start_(start), end_(end) {}

  const EOverlay start_;
  const EOverlay end_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSOverlayNonInterpolableValue);
template <>
struct DowncastTraits<CSSOverlayNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == CSSOverlayNonInterpolableValue::static_type_;
  }
};

class UnderlyingOverlayChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingOverlayChecker(EOverlay overlay) : overlay_(overlay) {}

  ~UnderlyingOverlayChecker() final = default;

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    double underlying_fraction =
        To<InterpolableNumber>(*underlying.interpolable_value)
            .Value(state.CssToLengthConversionData());
    EOverlay underlying_overlay =
        To<CSSOverlayNonInterpolableValue>(*underlying.non_interpolable_value)
            .Overlay(underlying_fraction);
    return overlay_ == underlying_overlay;
  }

  const EOverlay overlay_;
};

class InheritedOverlayChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedOverlayChecker(EOverlay overlay) : overlay_(overlay) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return overlay_ == state.ParentStyle()->Overlay();
  }

  const EOverlay overlay_;
};

InterpolationValue CSSOverlayInterpolationType::CreateOverlayValue(
    EOverlay overlay) const {
  return InterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(0),
      CSSOverlayNonInterpolableValue::Create(overlay, overlay));
}

InterpolationValue CSSOverlayInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  double underlying_fraction =
      To<InterpolableNumber>(*underlying.interpolable_value)
          .Value(CSSToLengthConversionData());
  EOverlay underlying_overlay =
      To<CSSOverlayNonInterpolableValue>(*underlying.non_interpolable_value)
          .Overlay(underlying_fraction);
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingOverlayChecker>(underlying_overlay));
  return CreateOverlayValue(underlying_overlay);
}

InterpolationValue CSSOverlayInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers&) const {
  return CreateOverlayValue(
      state.GetDocument().GetStyleResolver().InitialStyle().Overlay());
}

InterpolationValue CSSOverlayInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }
  EOverlay inherited_overlay = state.ParentStyle()->Overlay();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedOverlayChecker>(inherited_overlay));
  return CreateOverlayValue(inherited_overlay);
}

InterpolationValue CSSOverlayInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversion_checkers) const {
  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return nullptr;
  }

  CSSValueID keyword = identifier_value->GetValueID();

  switch (keyword) {
    case CSSValueID::kNone:
    case CSSValueID::kAuto:
      return CreateOverlayValue(identifier_value->ConvertTo<EOverlay>());
    default:
      return nullptr;
  }
}

InterpolationValue
CSSOverlayInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateOverlayValue(style.Overlay());
}

PairwiseInterpolationValue CSSOverlayInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  EOverlay start_overlay =
      To<CSSOverlayNonInterpolableValue>(*start.non_interpolable_value)
          .Overlay();
  EOverlay end_overlay =
      To<CSSOverlayNonInterpolableValue>(*end.non_interpolable_value).Overlay();
  return PairwiseInterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(0),
      MakeGarbageCollected<InterpolableNumber>(1),
      CSSOverlayNonInterpolableValue::Create(start_overlay, end_overlay));
}

void CSSOverlayInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(*this, value);
}

void CSSOverlayInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  // Overlay interpolation has been deferred to application time here due to
  // its non-linear behaviour.
  double fraction = To<InterpolableNumber>(interpolable_value)
                        .Value(state.CssToLengthConversionData());
  EOverlay overlay = To<CSSOverlayNonInterpolableValue>(non_interpolable_value)
                         ->Overlay(fraction);
  state.StyleBuilder().SetOverlay(overlay);
}

}  // namespace blink
