// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_dynamic_range_limit_interpolation_type.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/animation/interpolable_dynamic_range_limit.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class InheritedDynamicRangeLimitChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedDynamicRangeLimitChecker(DynamicRangeLimit limit)
      : limit_(limit) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return limit_ == state.ParentStyle()->GetDynamicRangeLimit();
  }

  DynamicRangeLimit limit_;
};

InterpolationValue
CSSDynamicRangeLimitInterpolationType::ConvertDynamicRangeLimit(
    DynamicRangeLimit limit) {
  return InterpolationValue(InterpolableDynamicRangeLimit::Create(limit));
}

InterpolationValue CSSDynamicRangeLimitInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue CSSDynamicRangeLimitInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  return ConvertDynamicRangeLimit(
      DynamicRangeLimit(cc::PaintFlags::DynamicRangeLimit::kHigh));
}

InterpolationValue CSSDynamicRangeLimitInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  DCHECK(state.ParentStyle());
  DynamicRangeLimit inherited_limit =
      state.ParentStyle()->GetDynamicRangeLimit();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedDynamicRangeLimitChecker>(inherited_limit));
  return ConvertDynamicRangeLimit(inherited_limit);
}

InterpolationValue CSSDynamicRangeLimitInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  return ConvertDynamicRangeLimit(
      StyleBuilderConverterBase::ConvertDynamicRangeLimit(value));
}

InterpolationValue CSSDynamicRangeLimitInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  return ConvertDynamicRangeLimit(style.GetDynamicRangeLimit());
}

void CSSDynamicRangeLimitInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableDynamicRangeLimit& interpolable_limit =
      To<InterpolableDynamicRangeLimit>(interpolable_value);

  state.StyleBuilder().SetDynamicRangeLimit(
      interpolable_limit.GetDynamicRangeLimit());
}

}  // namespace blink
