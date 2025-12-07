// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_basic_shape_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

struct BasicShapeInfo {
  STACK_ALLOCATED();

 public:
  const BasicShape* shape = nullptr;
  GeometryBox geometry_box = GeometryBox::kBorderBox;
  CoordBox coord_box = CoordBox::kBorderBox;
};

BasicShapeInfo GetBasicShapeInfo(const CSSProperty& property,
                                 const ComputedStyle& style) {
  BasicShapeInfo info;
  switch (property.PropertyID()) {
    case CSSPropertyID::kShapeOutside:
      if (!style.ShapeOutside())
        return info;
      if (style.ShapeOutside()->GetType() != ShapeValue::kShape)
        return info;
      if (style.ShapeOutside()->CssBox() != CSSBoxType::kMissing)
        return info;
      info.shape = style.ShapeOutside()->Shape();
      return info;
    case CSSPropertyID::kOffsetPath: {
      auto* offset_path_operation =
          DynamicTo<ShapeOffsetPathOperation>(style.OffsetPath());
      if (!offset_path_operation) {
        return info;
      }
      const auto& shape = offset_path_operation->GetBasicShape();

      // Path, Shape and Ray shapes are handled by PathInterpolationType,
      // ShapeInterpolationType and RayInterpolationType.
      if (shape.GetType() == BasicShape::kStylePathType ||
          shape.GetType() == BasicShape::kStyleRayType ||
          shape.GetType() == BasicShape::kStyleShapeType) {
        return info;
      }

      info.shape = &shape;
      info.coord_box = offset_path_operation->GetCoordBox();
      return info;
    }
    case CSSPropertyID::kClipPath: {
      auto* clip_path_operation =
          DynamicTo<ShapeClipPathOperation>(style.ClipPath());
      if (!clip_path_operation)
        return info;
      auto* shape = clip_path_operation->GetBasicShape();

      // Path shape is handled by PathInterpolationType.
      // Shape is handled by ShapeInterpolationType
      if (shape->GetType() == BasicShape::kStylePathType ||
          shape->GetType() == BasicShape::kStyleShapeType) {
        return info;
      }

      info.shape = shape;
      info.geometry_box = clip_path_operation->GetGeometryBox();
      return info;
    }
    case CSSPropertyID::kObjectViewBox:
      info.shape = style.ObjectViewBox();
      return info;
    default:
      NOTREACHED();
  }
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
      info.shape, CssProperty(), 1, info.geometry_box, info.coord_box);
}

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  auto info = GetBasicShapeInfo(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedShapeChecker>(CssProperty(), info.shape));
  return basic_shape_interpolation_functions::MaybeConvertBasicShape(
      info.shape, CssProperty(), state.ParentStyle()->EffectiveZoom(),
      info.geometry_box, info.coord_box);
}

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList()) {
    return basic_shape_interpolation_functions::MaybeConvertCSSValue(
        value, CssProperty(), GeometryBox::kBorderBox, CoordBox::kBorderBox);
  }

  const auto& list = To<CSSValueList>(value);
  const CSSValue& first = list.First();
  // Path, Shape and Ray shapes are handled by PathInterpolationType,
  // ShapeInterpolationType and RayInterpolationType.
  if (!first.IsBasicShapeValue() || first.IsRayValue() || first.IsPathValue() ||
      first.IsShapeValue()) {
    return nullptr;
  }
  GeometryBox geometry_box = GeometryBox::kBorderBox;
  CoordBox coord_box = CoordBox::kBorderBox;
  if (list.length() == 2) {
    const CSSValue& tail = list.Item(1);
    if (const auto* ident = DynamicTo<CSSIdentifierValue>(tail)) {
      if (CssProperty().PropertyID() == CSSPropertyID::kClipPath) {
        geometry_box = ident->ConvertTo<GeometryBox>();
      } else if (CssProperty().PropertyID() == CSSPropertyID::kOffsetPath) {
        coord_box = ident->ConvertTo<CoordBox>();
      }
    }
  }
  return basic_shape_interpolation_functions::MaybeConvertCSSValue(
      first, CssProperty(), geometry_box, coord_box);
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
      info.shape, CssProperty(), style.EffectiveZoom(), info.geometry_box,
      info.coord_box);
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
  BasicShape* shape = basic_shape_interpolation_functions::CreateBasicShape(
      interpolable_value, *non_interpolable_value,
      state.CssToLengthConversionData());
  switch (CssProperty().PropertyID()) {
    case CSSPropertyID::kShapeOutside:
      state.StyleBuilder().SetShapeOutside(
          MakeGarbageCollected<ShapeValue>(shape, CSSBoxType::kMissing));
      break;
    case CSSPropertyID::kOffsetPath: {
      CoordBox coord_box = CoordBox::kBorderBox;
      if (non_interpolable_value) {
        coord_box = basic_shape_interpolation_functions::GetCoordBox(
            *non_interpolable_value);
      }
      state.StyleBuilder().SetOffsetPath(
          shape
              ? MakeGarbageCollected<ShapeOffsetPathOperation>(shape, coord_box)
              : nullptr);
      break;
    }
    case CSSPropertyID::kClipPath: {
      GeometryBox geometry_box = GeometryBox::kBorderBox;
      if (non_interpolable_value) {
        geometry_box = basic_shape_interpolation_functions::GetGeometryBox(
            *non_interpolable_value);
      }
      state.StyleBuilder().SetClipPath(
          shape ? MakeGarbageCollected<ShapeClipPathOperation>(shape,
                                                               geometry_box)
                : nullptr);
      break;
    }
    case CSSPropertyID::kObjectViewBox:
      state.StyleBuilder().SetObjectViewBox(shape);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace blink
