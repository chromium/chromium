// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_transform_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolable_transform_list.h"
#include "third_party/blink/renderer/core/animation/length_units_checker.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/resolver/transform_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"

namespace blink {
namespace {
InterpolationValue ConvertTransform(TransformOperations&& transform) {
  return InterpolationValue(MakeGarbageCollected<InterpolableTransformList>(
      std::move(transform),
      TransformOperations::BoxSizeDependentMatrixBlending::kAllow));
}

InterpolationValue ConvertTransform(const TransformOperations& transform) {
  return ConvertTransform(TransformOperations(transform));
}

class InheritedTransformChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedTransformChecker(const TransformOperations& inherited_transform)
      : inherited_transform_(inherited_transform) {}

  void Trace(Visitor* visitor) const final {
    CSSConversionChecker::Trace(visitor);
    visitor->Trace(inherited_transform_);
  }

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return inherited_transform_ == state.ParentStyle()->Transform();
  }

 private:
  const TransformOperations inherited_transform_;
};

class AlwaysInvalidateChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return false;
  }
};
}  // namespace

InterpolationValue CSSTransformInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return ConvertTransform(EmptyTransformOperations());
}

InterpolationValue CSSTransformInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers&) const {
  return ConvertTransform(
      state.GetDocument().GetStyleResolver().InitialStyle().Transform());
}

InterpolationValue CSSTransformInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const TransformOperations& inherited_transform =
      state.ParentStyle()->Transform();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedTransformChecker>(inherited_transform));
  return ConvertTransform(inherited_transform);
}

InterpolationValue CSSTransformInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  CHECK(state);
  if (auto* list_value = DynamicTo<CSSValueList>(value)) {
    CSSPrimitiveValue::LengthTypeFlags types;
    for (const CSSValue* item : *list_value) {
      const auto& transform_function = To<CSSFunctionValue>(*item);
      if (transform_function.FunctionType() == CSSValueID::kMatrix ||
          transform_function.FunctionType() == CSSValueID::kMatrix3d) {
        types.set(CSSPrimitiveValue::kUnitTypePixels);
        continue;
      }
      for (const CSSValue* argument : transform_function) {
        // perspective(none) is an identifier value rather than a
        // primitive value, but since it represents infinity and
        // perspective() interpolates by reciprocals, it interpolates as
        // 0.
        const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(argument);
        DCHECK(primitive_value ||
               (transform_function.FunctionType() == CSSValueID::kPerspective &&
                argument->IsIdentifierValue()));
        if (!primitive_value ||
            (!primitive_value->IsLength() &&
             !primitive_value->IsCalculatedPercentageWithLength())) {
          continue;
        }
        primitive_value->AccumulateLengthUnitTypes(types);
      }
    }

    if (InterpolationType::ConversionChecker* length_units_checker =
            LengthUnitsChecker::MaybeCreate(types, *state)) {
      conversion_checkers.push_back(length_units_checker);
    }
  }

  return InterpolationValue(InterpolableTransformList::ConvertCSSValue(
      value, state->CssToLengthConversionData(),
      TransformOperations::BoxSizeDependentMatrixBlending::kAllow));
}

InterpolationValue
CSSTransformInterpolationType::PreInterpolationCompositeIfNeeded(
    InterpolationValue value,
    const InterpolationValue& underlying,
    EffectModel::CompositeOperation composite,
    ConversionCheckers& conversion_checkers) const {
  // Due to the post-interpolation composite optimization, the interpolation
  // stack aggressively caches interpolated values. When we are doing
  // pre-interpolation compositing, this can cause us to bake-in the composited
  // result even when the underlying value is changing. This checker is a hack
  // to disable that caching in this case.
  // TODO(crbug.com/1009230): Remove this once our interpolation code isn't
  // caching composited values.
  conversion_checkers.push_back(
      MakeGarbageCollected<AlwaysInvalidateChecker>());

  InterpolableTransformList& transform_list =
      To<InterpolableTransformList>(*value.interpolable_value);
  const InterpolableTransformList& underlying_transform_list =
      To<InterpolableTransformList>(*underlying.interpolable_value);

  // Addition of transform lists uses concatenation, whilst accumulation
  // performs a similar matching to interpolation but then adds the components.
  // See https://drafts.csswg.org/css-transforms-2/#combining-transform-lists
  if (composite == EffectModel::CompositeOperation::kCompositeAdd) {
    transform_list.PreConcat(underlying_transform_list);
  } else {
    DCHECK_EQ(composite, EffectModel::CompositeOperation::kCompositeAccumulate);
    transform_list.AccumulateOnto(underlying_transform_list);
  }
  return value;
}

PairwiseInterpolationValue CSSTransformInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  // We don't do any checking here; InterpolableTransformList::Interpolate will
  // handle discrete animation for us if needed.
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value));
}

InterpolationValue
CSSTransformInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertTransform(style.Transform());
}

void CSSTransformInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  // We do our compositing behavior in |PreInterpolationCompositeIfNeeded|; see
  // the documentation on that method.
  underlying_value_owner.Set(*this, value);
}

void CSSTransformInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* untyped_non_interpolable_value,
    StyleResolverState& state) const {
  state.StyleBuilder().SetTransform(
      To<InterpolableTransformList>(interpolable_value).operations());
}

}  // namespace blink
