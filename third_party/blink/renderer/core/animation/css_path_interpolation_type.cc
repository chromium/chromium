// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_path_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/animation/path_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/shape_property_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_value.h"

namespace blink {

namespace {

struct PathAndCssBox {
  STACK_ALLOCATED();

 public:
  const StylePath* path = nullptr;
  // Only set for shape-outside with an explicit <shape-box>.
  ShapeReferenceBox box;
};

// Returns the property's path() value (and shape-outside's <shape-box> if any).
// If the property's value is not a path(), `path` is nullptr.
PathAndCssBox GetPathAndCssBox(const CSSProperty& property,
                               const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kD:
      return {style.D()};
    case CSSPropertyID::kOffsetPath: {
      auto* shape = DynamicTo<ShapeOffsetPathOperation>(style.OffsetPath());
      if (!shape) {
        return {};
      }
      return {DynamicTo<StylePath>(shape->GetBasicShape())};
    }
    case CSSPropertyID::kClipPath: {
      auto* shape = DynamicTo<ShapeClipPathOperation>(style.ClipPath());
      if (!shape) {
        return {};
      }
      return {DynamicTo<StylePath>(shape->GetBasicShape())};
    }
    case CSSPropertyID::kShapeOutside: {
      const ShapeValue* shape_value = style.ShapeOutside();
      if (!shape_value || shape_value->GetType() != ShapeValue::kShape) {
        return {};
      }
      return {DynamicTo<StylePath>(shape_value->Shape()),
              {.shape = shape_value->CssBox()}};
    }
    default:
      NOTREACHED();
  }
}

// Set the property to the given path() value.
void SetPath(const CSSProperty& property,
             ComputedStyleBuilder& builder,
             blink::StylePath* path,
             ShapeReferenceBox box) {
  CHECK(path);
  switch (property.PropertyID()) {
    case CSSPropertyID::kD:
      builder.SetD(path);
      return;
    case CSSPropertyID::kOffsetPath:
      // TODO(sakhapov): handle coord box.
      builder.SetOffsetPath(MakeGarbageCollected<ShapeOffsetPathOperation>(
          *path, CoordBox::kBorderBox));
      return;
    case CSSPropertyID::kClipPath:
      // TODO(pdr): Handle geometry box.
      builder.SetClipPath(MakeGarbageCollected<ShapeClipPathOperation>(
          *path, GeometryBox::kBorderBox));
      return;
    case CSSPropertyID::kShapeOutside:
      builder.SetShapeOutside(MakeGarbageCollected<ShapeValue>(
          *path, box.shape.value_or(ShapeBox::kMarginBox)));
      return;
    default:
      NOTREACHED();
  }
}

}  // namespace

void CSSPathInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  CHECK(non_interpolable_value);
  SetPath(CssProperty(), state.StyleBuilder(),
          PathInterpolationFunctions::AppliedValue(interpolable_value,
                                                   *non_interpolable_value),
          PathInterpolationFunctions::GetBox(*non_interpolable_value));
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
      nullptr, PathInterpolationFunctions::kForceAbsolute, {});
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
    auto parent_info = GetPathAndCssBox(property_, *state.ParentStyle());
    return parent_info.path == style_path_.Get() && parent_info.box == box_;
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

  auto parent_info = GetPathAndCssBox(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(MakeGarbageCollected<InheritedPathChecker>(
      CssProperty(), parent_info.path, parent_info.box));
  return PathInterpolationFunctions::ConvertValue(
      parent_info.path, PathInterpolationFunctions::kForceAbsolute,
      parent_info.box);
}

InterpolationValue CSSPathInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  const cssvalue::CSSPathValue* path_value = nullptr;
  ShapeReferenceBox box;
  if (CssProperty().PropertyID() == CSSPropertyID::kShapeOutside) {
    box.shape = ShapeBox::kMarginBox;
  }
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    path_value = DynamicTo<cssvalue::CSSPathValue>(list->First());
    if (CssProperty().PropertyID() == CSSPropertyID::kShapeOutside &&
        list->length() == 2) {
      if (const auto* ident = DynamicTo<CSSIdentifierValue>(list->Last())) {
        box.shape = ident->ConvertTo<ShapeBox>();
      }
    }
  } else {
    path_value = DynamicTo<cssvalue::CSSPathValue>(value);
  }
  if (!path_value) {
    return nullptr;
  }
  return PathInterpolationFunctions::ConvertValue(
      path_value->GetStylePath(), PathInterpolationFunctions::kForceAbsolute,
      box);
}

InterpolationValue
CSSPathInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  auto info = GetPathAndCssBox(CssProperty(), style);
  return PathInterpolationFunctions::ConvertValue(
      info.path, PathInterpolationFunctions::kForceAbsolute, info.box);
}

PairwiseInterpolationValue CSSPathInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return PathInterpolationFunctions::MaybeMergeSingles(std::move(start),
                                                       std::move(end));
}

}  // namespace blink
