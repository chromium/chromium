// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/shape_interpolation_functions.h"

#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/css_shape_interpolation_type.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"

namespace blink {

namespace shape_interpolation_functions {

InterpolationValue MaybeConvertCSSValue(const CSSValue& value,
                                        const CSSProperty& property,
                                        GeometryBox geometry_box,
                                        CoordBox coord_box) {
  InterpolationValue result =
      basic_shape_interpolation_functions::MaybeConvertCSSValue(
          value, property, geometry_box, coord_box);
  if (result) {
    return result;
  }
  return CSSShapeInterpolationType::MaybeConvertCSSValue(
      value, property, geometry_box, coord_box);
}

InterpolationValue MaybeConvertBasicShape(const BasicShape* shape,
                                          const CSSProperty& property,
                                          double zoom,
                                          GeometryBox geometry_box,
                                          CoordBox coord_box) {
  if (!shape) {
    return nullptr;
  }
  switch (shape->GetType()) {
    case BasicShape::kStylePathType:
    case BasicShape::kStyleShapeType:
      return CSSShapeInterpolationType::MaybeConvertBasicShape(
          shape, property, zoom, geometry_box, coord_box);
    default:
      return basic_shape_interpolation_functions::MaybeConvertBasicShape(
          shape, property, zoom, geometry_box, coord_box);
  }
}

bool ShapesAreCompatible(const NonInterpolableValue& a,
                         const NonInterpolableValue& b) {
  if (a.GetType() ==
          CSSShapeInterpolationType::ShapeNonInterpolableValueType() &&
      b.GetType() ==
          CSSShapeInterpolationType::ShapeNonInterpolableValueType()) {
    return CSSShapeInterpolationType::ShapesAreCompatible(a, b);
  }
  if (a.GetType() ==
          CSSShapeInterpolationType::ShapeNonInterpolableValueType() ||
      b.GetType() ==
          CSSShapeInterpolationType::ShapeNonInterpolableValueType()) {
    return false;
  }
  return basic_shape_interpolation_functions::ShapesAreCompatible(a, b);
}

InterpolableValue* CreateNeutralValue(
    const NonInterpolableValue& non_interpolable) {
  if (non_interpolable.GetType() ==
      CSSShapeInterpolationType::ShapeNonInterpolableValueType()) {
    return CSSShapeInterpolationType::CreateNeutralValue(non_interpolable);
  }
  return basic_shape_interpolation_functions::CreateNeutralValue(
      non_interpolable);
}

BasicShape* CreateBasicShape(const InterpolableValue& interpolable_value,
                             const NonInterpolableValue& non_interpolable,
                             const CSSToLengthConversionData& conversion_data) {
  if (non_interpolable.GetType() ==
      CSSShapeInterpolationType::ShapeNonInterpolableValueType()) {
    return CSSShapeInterpolationType::CreateShape(
        interpolable_value, &non_interpolable, conversion_data);
  }
  return basic_shape_interpolation_functions::CreateBasicShape(
      interpolable_value, non_interpolable, conversion_data);
}

GeometryBox GetGeometryBox(const NonInterpolableValue& value,
                           GeometryBox default_box) {
  if (value.GetType() ==
      CSSShapeInterpolationType::ShapeNonInterpolableValueType()) {
    return CSSShapeInterpolationType::GetGeometryBox(value).value_or(
        default_box);
  }
  return basic_shape_interpolation_functions::GetGeometryBox(value,
                                                             default_box);
}

CoordBox GetCoordBox(const NonInterpolableValue& value) {
  if (value.GetType() ==
      CSSShapeInterpolationType::ShapeNonInterpolableValueType()) {
    return CSSShapeInterpolationType::GetCoordBox(value).value_or(
        CoordBox::kBorderBox);
  }
  return basic_shape_interpolation_functions::GetCoordBox(value);
}

}  // namespace shape_interpolation_functions

}  // namespace blink
