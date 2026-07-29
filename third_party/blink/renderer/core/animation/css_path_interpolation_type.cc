// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_path_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/path_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/shape_property_functions.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

// Returns the property's path() value (and any associated reference box). If
// the property's value is not a path(), the shape will be null.
BasicShapeInfo GetPathInfo(const CSSProperty& property,
                           const ComputedStyle& style) {
  BasicShapeInfo info =
      shape_property_functions::GetBasicShape(property, style);
  if (IsA<StylePath>(info.shape)) {
    return info;
  }
  return {};
}

}  // namespace

void CSSPathInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  CHECK(non_interpolable_value);
  BasicShapeInfo info = PathInterpolationFunctions::AppliedValue(
      interpolable_value, *non_interpolable_value);
  shape_property_functions::SetBasicShape(CssProperty(), info,
                                          state.StyleBuilder());
}

void CSSPathInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  PathInterpolationFunctions::Composite(underlying_value_owner,
                                        underlying_fraction, this, value);
}

InterpolationValue CSSPathInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  return PathInterpolationFunctions::MaybeConvertNeutral(underlying,
                                                         conversion_checkers);
}

InterpolationValue CSSPathInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return PathInterpolationFunctions::ConvertValue(
      {}, PathInterpolationFunctions::kForceAbsolute);
}

class InheritedPathChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedPathChecker(const CSSProperty& property,
                       const StylePath* style_path,
                       ShapeReferenceBox box)
      : property_(property), style_path_(style_path), box_(box) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(style_path_);
    CSSInterpolationType::CSSConversionChecker::Trace(visitor);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    auto parent_info = GetPathInfo(property_, *state.ParentStyle());
    return parent_info.shape == style_path_.Get() && parent_info.box == box_;
  }

  const CSSProperty& property_;
  const Member<const StylePath> style_path_;
  const ShapeReferenceBox box_;
};

InterpolationValue CSSPathInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;

  auto parent_info = GetPathInfo(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(MakeGarbageCollected<InheritedPathChecker>(
      CssProperty(), To<StylePath>(parent_info.shape), parent_info.box));
  return PathInterpolationFunctions::ConvertValue(
      parent_info, PathInterpolationFunctions::kForceAbsolute);
}

InterpolationValue CSSPathInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  BasicShapeCssInfo css_info =
      shape_property_functions::GetCssBasicShape(CssProperty(), value);
  const auto* path_value = DynamicTo<cssvalue::CSSPathValue>(*css_info.shape);
  if (!path_value) {
    return nullptr;
  }
  return PathInterpolationFunctions::ConvertValue(
      {path_value->GetStylePath(), css_info.box},
      PathInterpolationFunctions::kForceAbsolute);
}

InterpolationValue
CSSPathInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  auto info = GetPathInfo(CssProperty(), style);
  return PathInterpolationFunctions::ConvertValue(
      info, PathInterpolationFunctions::kForceAbsolute);
}

PairwiseInterpolationValue CSSPathInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return PathInterpolationFunctions::MaybeMergeSingles(std::move(start),
                                                       std::move(end));
}

}  // namespace blink
