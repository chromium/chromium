/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/resolver/transform_builder.h"

#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/perspective_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/skew_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

static Length ConvertToFloatLength(
    const CSSPrimitiveValue& primitive_value,
    const CSSToLengthConversionData& conversion_data) {
  return primitive_value.ConvertToLength(conversion_data);
}

static TransformOperation::OperationType GetTransformOperationType(
    CSSValueID type) {
  switch (type) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case CSSValueID::kScale:
      return TransformOperation::kScale;
    case CSSValueID::kScaleX:
      return TransformOperation::kScaleX;
    case CSSValueID::kScaleY:
      return TransformOperation::kScaleY;
    case CSSValueID::kScaleZ:
      return TransformOperation::kScaleZ;
    case CSSValueID::kScale3d:
      return TransformOperation::kScale3D;
    case CSSValueID::kTranslate:
      return TransformOperation::kTranslate;
    case CSSValueID::kTranslateX:
      return TransformOperation::kTranslateX;
    case CSSValueID::kTranslateY:
      return TransformOperation::kTranslateY;
    case CSSValueID::kTranslateZ:
      return TransformOperation::kTranslateZ;
    case CSSValueID::kTranslate3d:
      return TransformOperation::kTranslate3D;
    case CSSValueID::kRotate:
      return TransformOperation::kRotate;
    case CSSValueID::kRotateX:
      return TransformOperation::kRotateX;
    case CSSValueID::kRotateY:
      return TransformOperation::kRotateY;
    case CSSValueID::kRotateZ:
      return TransformOperation::kRotateZ;
    case CSSValueID::kRotate3d:
      return TransformOperation::kRotate3D;
    case CSSValueID::kSkew:
      return TransformOperation::kSkew;
    case CSSValueID::kSkewX:
      return TransformOperation::kSkewX;
    case CSSValueID::kSkewY:
      return TransformOperation::kSkewY;
    case CSSValueID::kMatrix:
      return TransformOperation::kMatrix;
    case CSSValueID::kMatrix3d:
      return TransformOperation::kMatrix3D;
    case CSSValueID::kPerspective:
      return TransformOperation::kPerspective;
  }
}

bool TransformBuilder::HasRelativeLengths(const CSSValueList& value_list) {
  for (auto& value : value_list) {
    const auto* transform_value = To<CSSFunctionValue>(value.Get());

    for (const CSSValue* item : *transform_value) {
      const auto& primitive_value = To<CSSPrimitiveValue>(*item);
      if (primitive_value.IsCalculated()) {
        if (To<CSSMathFunctionValue>(primitive_value).MayHaveRelativeUnit()) {
          return true;
        }
      } else {
        CSSPrimitiveValue::UnitType unit_type =
            To<CSSNumericLiteralValue>(primitive_value).GetType();
        if (CSSPrimitiveValue::IsRelativeUnit(unit_type)) {
          return true;
        }
      }
    }
  }
  return false;
}

namespace {

TransformOperation* CreateTransformOperation(
    const CSSFunctionValue& transform_value,
    const CSSToLengthConversionData& conversion_data) {
  TransformOperation::OperationType transform_type =
      GetTransformOperationType(transform_value.FunctionType());
  switch (transform_type) {
    case TransformOperation::kScale:
    case TransformOperation::kScaleX:
    case TransformOperation::kScaleY: {
      const auto& first_value = To<CSSPrimitiveValue>(transform_value.Item(0));
      double sx = 1.0;
      double sy = 1.0;
      if (transform_type == TransformOperation::kScaleY) {
        sy = first_value.ComputeNumber(conversion_data);
      } else {
        sx = first_value.ComputeNumber(conversion_data);
        if (transform_type != TransformOperation::kScaleX) {
          if (transform_value.length() > 1) {
            const auto& second_value =
                To<CSSPrimitiveValue>(transform_value.Item(1));
            sy = second_value.ComputeNumber(conversion_data);
          } else {
            sy = sx;
          }
        }
      }
      return MakeGarbageCollected<ScaleTransformOperation>(sx, sy, 1.0,
                                                           transform_type);
    }
    case TransformOperation::kScaleZ:
    case TransformOperation::kScale3D: {
      const auto& first_value = To<CSSPrimitiveValue>(transform_value.Item(0));
      double sx = 1.0;
      double sy = 1.0;
      double sz = 1.0;
      if (transform_type == TransformOperation::kScaleZ) {
        sz = first_value.ComputeNumber(conversion_data);
      } else {
        sx = first_value.ComputeNumber(conversion_data);
        sy = To<CSSPrimitiveValue>(transform_value.Item(1))
                 .ComputeNumber(conversion_data);
        sz = To<CSSPrimitiveValue>(transform_value.Item(2))
                 .ComputeNumber(conversion_data);
      }
      return MakeGarbageCollected<ScaleTransformOperation>(sx, sy, sz,
                                                           transform_type);
    }
    case TransformOperation::kTranslate:
    case TransformOperation::kTranslateX:
    case TransformOperation::kTranslateY: {
      const auto& first_value = To<CSSPrimitiveValue>(transform_value.Item(0));
      Length tx = Length::Fixed(0);
      Length ty = Length::Fixed(0);
      if (transform_type == TransformOperation::kTranslateY) {
        ty = ConvertToFloatLength(first_value, conversion_data);
      } else {
        tx = ConvertToFloatLength(first_value, conversion_data);
        if (transform_type != TransformOperation::kTranslateX) {
          if (transform_value.length() > 1) {
            const auto& second_value =
                To<CSSPrimitiveValue>(transform_value.Item(1));
            ty = ConvertToFloatLength(second_value, conversion_data);
          }
        }
      }
      return MakeGarbageCollected<TranslateTransformOperation>(tx, ty, 0,
                                                               transform_type);
    }
    case TransformOperation::kTranslateZ:
    case TransformOperation::kTranslate3D: {
      const auto& first_value = To<CSSPrimitiveValue>(transform_value.Item(0));
      Length tx = Length::Fixed(0);
      Length ty = Length::Fixed(0);
      double tz = 0;
      if (transform_type == TransformOperation::kTranslateZ) {
        tz = first_value.ComputeLength<double>(conversion_data);
      } else {
        tx = ConvertToFloatLength(first_value, conversion_data);
        ty = ConvertToFloatLength(
            To<CSSPrimitiveValue>(transform_value.Item(1)), conversion_data);
        tz = To<CSSPrimitiveValue>(transform_value.Item(2))
                 .ComputeLength<double>(conversion_data);
      }

      return MakeGarbageCollected<TranslateTransformOperation>(tx, ty, tz,
                                                               transform_type);
    }
    case TransformOperation::kRotateX:
    case TransformOperation::kRotateY:
    case TransformOperation::kRotateZ:
    case TransformOperation::kRotate: {
      const auto& first_value = To<CSSPrimitiveValue>(transform_value.Item(0));
      double angle = first_value.ComputeDegrees(conversion_data);
      if (transform_value.length() == 1) {
        double x = transform_type == TransformOperation::kRotateX;
        double y = transform_type == TransformOperation::kRotateY;
        double z = transform_type == TransformOperation::kRotateZ ||
                   transform_type == TransformOperation::kRotate;
        return MakeGarbageCollected<RotateTransformOperation>(x, y, z, angle,
                                                              transform_type);
      } else {
        // For SVG 'transform' attributes we generate 3-argument rotate()
        // functions.
        DCHECK_EQ(transform_value.length(), 3u);
        const auto& second_value =
            To<CSSPrimitiveValue>(transform_value.Item(1));
        const CSSPrimitiveValue& third_value =
            To<CSSPrimitiveValue>(transform_value.Item(2));
        return MakeGarbageCollected<RotateAroundOriginTransformOperation>(
            angle, second_value.ComputeLength<double>(conversion_data),
            third_value.ComputeLength<double>(conversion_data));
      }
    }
    case TransformOperation::kRotate3D: {
      const auto& first_value = To<CSSPrimitiveValue>(transform_value.Item(0));
      const auto& second_value = To<CSSPrimitiveValue>(transform_value.Item(1));
      const auto& third_value = To<CSSPrimitiveValue>(transform_value.Item(2));
      const auto& fourth_value = To<CSSPrimitiveValue>(transform_value.Item(3));
      double x = first_value.ComputeNumber(conversion_data);
      double y = second_value.ComputeNumber(conversion_data);
      double z = third_value.ComputeNumber(conversion_data);
      double angle = fourth_value.ComputeDegrees(conversion_data);
      return MakeGarbageCollected<RotateTransformOperation>(x, y, z, angle,
                                                            transform_type);
    }
    case TransformOperation::kSkew:
    case TransformOperation::kSkewX:
    case TransformOperation::kSkewY: {
      const auto& first_value = To<CSSPrimitiveValue>(transform_value.Item(0));
      double angle_x = 0;
      double angle_y = 0;
      double angle = first_value.ComputeDegrees(conversion_data);
      if (transform_type == TransformOperation::kSkewY) {
        angle_y = angle;
      } else {
        angle_x = angle;
        if (transform_type == TransformOperation::kSkew) {
          if (transform_value.length() > 1) {
            const auto& second_value =
                To<CSSPrimitiveValue>(transform_value.Item(1));
            angle_y = second_value.ComputeDegrees(conversion_data);
          }
        }
      }
      return MakeGarbageCollected<SkewTransformOperation>(angle_x, angle_y,
                                                          transform_type);
    }
    case TransformOperation::kMatrix: {
      double a = To<CSSPrimitiveValue>(transform_value.Item(0))
                     .ComputeNumber(conversion_data);
      double b = To<CSSPrimitiveValue>(transform_value.Item(1))
                     .ComputeNumber(conversion_data);
      double c = To<CSSPrimitiveValue>(transform_value.Item(2))
                     .ComputeNumber(conversion_data);
      double d = To<CSSPrimitiveValue>(transform_value.Item(3))
                     .ComputeNumber(conversion_data);
      double e = conversion_data.Zoom() *
                 To<CSSPrimitiveValue>(transform_value.Item(4))
                     .ComputeNumber(conversion_data);
      double f = conversion_data.Zoom() *
                 To<CSSPrimitiveValue>(transform_value.Item(5))
                     .ComputeNumber(conversion_data);
      return MakeGarbageCollected<MatrixTransformOperation>(a, b, c, d, e, f);
    }
    case TransformOperation::kMatrix3D: {
      auto matrix = gfx::Transform::ColMajor(
          To<CSSPrimitiveValue>(transform_value.Item(0))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(1))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(2))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(3))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(4))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(5))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(6))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(7))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(8))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(9))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(10))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(11))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(12))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(13))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(14))
              .ComputeNumber(conversion_data),
          To<CSSPrimitiveValue>(transform_value.Item(15))
              .ComputeNumber(conversion_data));
      matrix.Zoom(conversion_data.Zoom());
      return MakeGarbageCollected<Matrix3DTransformOperation>(matrix);
    }
    case TransformOperation::kPerspective: {
      std::optional<double> p;
      const auto& first_value = transform_value.Item(0);
      const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(first_value);
      if (primitive_value) {
        p = primitive_value->ComputeLength<double>(conversion_data);
        DCHECK_GE(*p, 0);
      } else {
        DCHECK_EQ(To<CSSIdentifierValue>(first_value).GetValueID(),
                  CSSValueID::kNone);
        // leave p as nullopt to represent 'none'
      }
      return MakeGarbageCollected<PerspectiveTransformOperation>(p);
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

}  // namespace

TransformOperations TransformBuilder::CreateTransformOperations(
    const CSSValue& in_value,
    const CSSToLengthConversionData& conversion_data) {
  TransformOperations operations;
  if (auto* in_value_function = DynamicTo<CSSFunctionValue>(in_value)) {
    operations.Operations().push_back(
        CreateTransformOperation(*in_value_function, conversion_data));
  } else if (auto* in_value_list = DynamicTo<CSSValueList>(in_value)) {
    for (auto& value : *in_value_list) {
      const auto* transform_value = To<CSSFunctionValue>(value.Get());
      operations.Operations().push_back(
          CreateTransformOperation(*transform_value, conversion_data));
    }
  } else {
    DCHECK_EQ(To<CSSIdentifierValue>(in_value).GetValueID(), CSSValueID::kNone);
  }
  return operations;
}

}  // namespace blink
