// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_path_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/path_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"

namespace blink {

namespace {

// Returns the property's path() value.
// If the property's value is not a path(), returns nullptr.
const StylePath* GetPath(const CSSProperty& property,
                         const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kD:
      return style.D();
    case CSSPropertyID::kOffsetPath: {
      auto* shape = DynamicTo<ShapeOffsetPathOperation>(style.OffsetPath());
      if (!shape) {
        return nullptr;
      }
      return DynamicTo<StylePath>(shape->GetBasicShape());
    }
    case CSSPropertyID::kClipPath: {
      auto* shape = DynamicTo<ShapeClipPathOperation>(style.ClipPath());
      if (!shape)
        return nullptr;
      return DynamicTo<StylePath>(shape->GetBasicShape());
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

// Set the property to the given path() value.
void SetPath(const CSSProperty& property,
             ComputedStyleBuilder& builder,
             scoped_refptr<blink::StylePath> path) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kD:
      builder.SetD(std::move(path));
      return;
    case CSSPropertyID::kOffsetPath:
      // TODO(sakhapov): handle coord box.
      builder.SetOffsetPath(MakeGarbageCollected<ShapeOffsetPathOperation>(
          std::move(path), CoordBox::kBorderBox));
      return;
    case CSSPropertyID::kClipPath:
      // TODO(pdr): Handle geometry box.
      builder.SetClipPath(MakeGarbageCollected<ShapeClipPathOperation>(
          std::move(path), GeometryBox::kBorderBox));
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

}  // namespace

void CSSPathInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  SetPath(CssProperty(), state.StyleBuilder(),
          PathInterpolationFunctions::AppliedValue(interpolable_value,
                                                   non_interpolable_value));
}

void CSSPathInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  PathInterpolationFunctions::Composite(underlying_value_owner,
                                        underlying_fraction, *this, value);
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
      nullptr, PathInterpolationFunctions::kForceAbsolute);
}

class InheritedPathChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedPathChecker(const CSSProperty& property,
                       scoped_refptr<const StylePath> style_path)
      : property_(property), style_path_(std::move(style_path)) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return GetPath(property_, *state.ParentStyle()) == style_path_.get();
  }

  const CSSProperty& property_;
  const scoped_refptr<const StylePath> style_path_;
};

InterpolationValue CSSPathInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;

  conversion_checkers.push_back(MakeGarbageCollected<InheritedPathChecker>(
      CssProperty(), GetPath(CssProperty(), *state.ParentStyle())));
  return PathInterpolationFunctions::ConvertValue(
      GetPath(CssProperty(), *state.ParentStyle()),
      PathInterpolationFunctions::kForceAbsolute);
}

InterpolationValue CSSPathInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  const cssvalue::CSSPathValue* path_value = nullptr;
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    path_value = DynamicTo<cssvalue::CSSPathValue>(list->First());
  } else {
    path_value = DynamicTo<cssvalue::CSSPathValue>(value);
  }
  if (!path_value) {
    return nullptr;
  }
  return PathInterpolationFunctions::ConvertValue(
      path_value->GetStylePath(), PathInterpolationFunctions::kForceAbsolute);
}

InterpolationValue
CSSPathInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return PathInterpolationFunctions::ConvertValue(
      GetPath(CssProperty(), style),
      PathInterpolationFunctions::kForceAbsolute);
}

PairwiseInterpolationValue CSSPathInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return PathInterpolationFunctions::MaybeMergeSingles(std::move(start),
                                                       std::move(end));
}

}  // namespace blink
