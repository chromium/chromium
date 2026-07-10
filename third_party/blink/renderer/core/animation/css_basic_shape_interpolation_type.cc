// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_basic_shape_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/shape_property_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

BasicShapeInfo GetBasicShapeInfo(const CSSProperty& property,
                                 const ComputedStyle& style) {
  BasicShapeInfo info =
      shape_property_functions::GetBasicShape(property, style);
  if (!info.shape) {
    return {};
  }
  switch (info.shape->GetType()) {
    case BasicShape::kBasicShapeCircleType:
    case BasicShape::kBasicShapeEllipseType:
    case BasicShape::kBasicShapeInsetType:
    case BasicShape::kBasicShapePolygonType:
      break;
    default:
      return {};
  }
  return info;
}

class UnderlyingCompatibilityChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingCompatibilityChecker(
      const NonInterpolableValue* underlying_non_interpolable_value)
      : underlying_non_interpolable_value_(underlying_non_interpolable_value) {}

  void Trace(Visitor* visitor) const override {
    CSSInterpolationType::CSSConversionChecker::Trace(visitor);
    visitor->Trace(underlying_non_interpolable_value_);
  }

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return basic_shape_interpolation_functions::ShapesAreCompatible(
        *underlying_non_interpolable_value_,
        *underlying.non_interpolable_value);
  }

  Member<const NonInterpolableValue> underlying_non_interpolable_value_;
};

class InheritedShapeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedShapeChecker(const CSSProperty& property,
                        const BasicShape* inherited_shape)
      : property_(property), inherited_shape_(inherited_shape) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(inherited_shape_);
    CSSInterpolationType::CSSConversionChecker::Trace(visitor);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return base::ValuesEquivalent(
        inherited_shape_.Get(),
        GetBasicShapeInfo(property_, *state.ParentStyle()).shape);
  }

  const CSSProperty& property_;
  Member<const BasicShape> inherited_shape_;
};

}  // namespace

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  // const_cast is for taking refs.
  NonInterpolableValue* non_interpolable_value =
      const_cast<NonInterpolableValue*>(
          underlying.non_interpolable_value.Get());
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingCompatibilityChecker>(
          non_interpolable_value));
  return InterpolationValue(
      basic_shape_interpolation_functions::CreateNeutralValue(
          *underlying.non_interpolable_value),
      non_interpolable_value);
}

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers&) const {
  const ComputedStyle& initial_style =
      state.GetDocument().GetStyleResolver().InitialStyle();
  auto info = GetBasicShapeInfo(CssProperty(), initial_style);
  return basic_shape_interpolation_functions::MaybeConvertBasicShape(
      info.shape, CssProperty(), 1, info.box);
}

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  auto info = GetBasicShapeInfo(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedShapeChecker>(CssProperty(), info.shape));
  return basic_shape_interpolation_functions::MaybeConvertBasicShape(
      info.shape, CssProperty(), state.ParentStyle()->EffectiveZoom(),
      info.box);
}

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  BasicShapeCssInfo css_info =
      shape_property_functions::GetCssBasicShape(CssProperty(), value);
  return basic_shape_interpolation_functions::MaybeConvertCSSValue(
      *css_info.shape, CssProperty(), css_info.box);
}

PairwiseInterpolationValue CSSBasicShapeInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (!basic_shape_interpolation_functions::ShapesAreCompatible(
          *start.non_interpolable_value, *end.non_interpolable_value))
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue
CSSBasicShapeInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  auto info = GetBasicShapeInfo(CssProperty(), style);
  return basic_shape_interpolation_functions::MaybeConvertBasicShape(
      info.shape, CssProperty(), style.EffectiveZoom(), info.box);
}

void CSSBasicShapeInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (!basic_shape_interpolation_functions::ShapesAreCompatible(
          *underlying_value_owner.Value().non_interpolable_value,
          *value.non_interpolable_value)) {
    underlying_value_owner.Set(this, value);
    return;
  }

  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

void CSSBasicShapeInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  CHECK(non_interpolable_value);
  BasicShape* shape = basic_shape_interpolation_functions::CreateBasicShape(
      interpolable_value, *non_interpolable_value,
      state.CssToLengthConversionData());
  CHECK(shape);
  shape_property_functions::SetBasicShape(
      CssProperty(), *shape,
      basic_shape_interpolation_functions::GetBox(*non_interpolable_value),
      state.StyleBuilder());
}

}  // namespace blink
