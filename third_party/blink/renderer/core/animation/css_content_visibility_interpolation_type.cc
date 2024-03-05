// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_content_visibility_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSContentVisibilityNonInterpolableValue final
    : public NonInterpolableValue {
 public:
  ~CSSContentVisibilityNonInterpolableValue() final = default;

  static scoped_refptr<CSSContentVisibilityNonInterpolableValue> Create(
      EContentVisibility start,
      EContentVisibility end) {
    return base::AdoptRef(
        new CSSContentVisibilityNonInterpolableValue(start, end));
  }

  EContentVisibility ContentVisibility() const {
    DCHECK_EQ(start_, end_);
    return start_;
  }

  EContentVisibility ContentVisibility(double fraction) const {
    if ((start_ == EContentVisibility::kHidden ||
         end_ == EContentVisibility::kHidden) &&
        start_ != end_) {
      // No halfway transition when transitioning to or from
      // content-visibility:hidden
      if (start_ == EContentVisibility::kHidden) {
        return fraction > 0 ? end_ : start_;
      } else {
        return fraction >= 1 ? end_ : start_;
      }
    }
    return fraction >= 0.5 ? end_ : start_;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSContentVisibilityNonInterpolableValue(EContentVisibility start,
                                           EContentVisibility end)
      : start_(start), end_(end) {}

  const EContentVisibility start_;
  const EContentVisibility end_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSContentVisibilityNonInterpolableValue);
template <>
struct DowncastTraits<CSSContentVisibilityNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() ==
           CSSContentVisibilityNonInterpolableValue::static_type_;
  }
};

class UnderlyingContentVisibilityChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingContentVisibilityChecker(
      EContentVisibility content_visibility)
      : content_visibility_(content_visibility) {}

  ~UnderlyingContentVisibilityChecker() final = default;

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    double underlying_fraction =
        To<InterpolableNumber>(*underlying.interpolable_value)
            .Value(state.CssToLengthConversionData());
    EContentVisibility underlying_content_visibility =
        To<CSSContentVisibilityNonInterpolableValue>(
            *underlying.non_interpolable_value)
            .ContentVisibility(underlying_fraction);
    return content_visibility_ == underlying_content_visibility;
  }

  const EContentVisibility content_visibility_;
};

class InheritedContentVisibilityChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedContentVisibilityChecker(
      EContentVisibility content_visibility)
      : content_visibility_(content_visibility) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return content_visibility_ == state.ParentStyle()->ContentVisibility();
  }

  const EContentVisibility content_visibility_;
};

InterpolationValue
CSSContentVisibilityInterpolationType::CreateContentVisibilityValue(
    EContentVisibility content_visibility) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(0),
                            CSSContentVisibilityNonInterpolableValue::Create(
                                content_visibility, content_visibility));
}

InterpolationValue CSSContentVisibilityInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  double underlying_fraction =
      To<InterpolableNumber>(*underlying.interpolable_value)
          .Value(CSSToLengthConversionData());
  EContentVisibility underlying_content_visibility =
      To<CSSContentVisibilityNonInterpolableValue>(
          *underlying.non_interpolable_value)
          .ContentVisibility(underlying_fraction);
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingContentVisibilityChecker>(
          underlying_content_visibility));
  return CreateContentVisibilityValue(underlying_content_visibility);
}

InterpolationValue CSSContentVisibilityInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return CreateContentVisibilityValue(EContentVisibility::kVisible);
}

InterpolationValue CSSContentVisibilityInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return nullptr;
  }
  EContentVisibility inherited_content_visibility =
      state.ParentStyle()->ContentVisibility();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedContentVisibilityChecker>(
          inherited_content_visibility));
  return CreateContentVisibilityValue(inherited_content_visibility);
}

InterpolationValue CSSContentVisibilityInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversion_checkers) const {
  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return nullptr;
  }

  CSSValueID keyword = identifier_value->GetValueID();

  switch (keyword) {
    case CSSValueID::kVisible:
    case CSSValueID::kHidden:
    case CSSValueID::kAuto:
      return CreateContentVisibilityValue(
          identifier_value->ConvertTo<EContentVisibility>());
    default:
      return nullptr;
  }
}

InterpolationValue CSSContentVisibilityInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  return CreateContentVisibilityValue(style.ContentVisibility());
}

PairwiseInterpolationValue
CSSContentVisibilityInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  EContentVisibility start_content_visibility =
      To<CSSContentVisibilityNonInterpolableValue>(
          *start.non_interpolable_value)
          .ContentVisibility();
  EContentVisibility end_content_visibility =
      To<CSSContentVisibilityNonInterpolableValue>(*end.non_interpolable_value)
          .ContentVisibility();
  return PairwiseInterpolationValue(
      MakeGarbageCollected<InterpolableNumber>(0),
      MakeGarbageCollected<InterpolableNumber>(1),
      CSSContentVisibilityNonInterpolableValue::Create(start_content_visibility,
                                                       end_content_visibility));
}

void CSSContentVisibilityInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(*this, value);
}

void CSSContentVisibilityInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  // ContentVisibility interpolation has been deferred to application time here
  // due to its non-linear behaviour.
  double fraction = To<InterpolableNumber>(interpolable_value)
                        .Value(state.CssToLengthConversionData());
  EContentVisibility content_visibility =
      To<CSSContentVisibilityNonInterpolableValue>(non_interpolable_value)
          ->ContentVisibility(fraction);
  state.StyleBuilder().SetContentVisibility(content_visibility);
}

}  // namespace blink
