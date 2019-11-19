/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_transform.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

SVGTransform::SVGTransform()
    : transform_type_(SVGTransformType::kUnknown), angle_(0) {}

SVGTransform::SVGTransform(SVGTransformType transform_type,
                           ConstructionMode mode)
    : transform_type_(transform_type), angle_(0) {
  if (mode == kConstructZeroTransform)
    matrix_ = AffineTransform(0, 0, 0, 0, 0, 0);
}

SVGTransform::SVGTransform(const AffineTransform& matrix)
    : transform_type_(SVGTransformType::kMatrix), angle_(0), matrix_(matrix) {}

SVGTransform::SVGTransform(SVGTransformType transform_type,
                           float angle,
                           const FloatPoint& center,
                           const AffineTransform& matrix)
    : transform_type_(transform_type),
      angle_(angle),
      center_(center),
      matrix_(matrix) {}

SVGTransform::~SVGTransform() = default;

SVGTransform* SVGTransform::Clone() const {
  return MakeGarbageCollected<SVGTransform>(transform_type_, angle_, center_,
                                            matrix_);
}

SVGPropertyBase* SVGTransform::CloneForAnimation(const String&) const {
  // SVGTransform is never animated.
  NOTREACHED();
  return nullptr;
}

void SVGTransform::SetMatrix(const AffineTransform& matrix) {
  OnMatrixChange();
  matrix_ = matrix;
}

void SVGTransform::OnMatrixChange() {
  transform_type_ = SVGTransformType::kMatrix;
  angle_ = 0;
}

void SVGTransform::SetTranslate(float tx, float ty) {
  transform_type_ = SVGTransformType::kTranslate;
  angle_ = 0;

  matrix_.MakeIdentity();
  matrix_.Translate(tx, ty);
}

FloatPoint SVGTransform::Translate() const {
  return FloatPoint::NarrowPrecision(matrix_.E(), matrix_.F());
}

void SVGTransform::SetScale(float sx, float sy) {
  transform_type_ = SVGTransformType::kScale;
  angle_ = 0;
  center_ = FloatPoint();

  matrix_.MakeIdentity();
  matrix_.ScaleNonUniform(sx, sy);
}

FloatSize SVGTransform::Scale() const {
  return FloatSize::NarrowPrecision(matrix_.A(), matrix_.D());
}

void SVGTransform::SetRotate(float angle, float cx, float cy) {
  transform_type_ = SVGTransformType::kRotate;
  angle_ = angle;
  center_ = FloatPoint(cx, cy);

  // TODO: toString() implementation, which can show cx, cy (need to be stored?)
  matrix_.MakeIdentity();
  matrix_.Translate(cx, cy);
  matrix_.Rotate(angle);
  matrix_.Translate(-cx, -cy);
}

void SVGTransform::SetSkewX(float angle) {
  transform_type_ = SVGTransformType::kSkewx;
  angle_ = angle;

  matrix_.MakeIdentity();
  matrix_.SkewX(angle);
}

void SVGTransform::SetSkewY(float angle) {
  transform_type_ = SVGTransformType::kSkewy;
  angle_ = angle;

  matrix_.MakeIdentity();
  matrix_.SkewY(angle);
}

namespace {

const char* TransformTypePrefixForParsing(SVGTransformType type) {
  switch (type) {
    case SVGTransformType::kUnknown:
      return "";
    case SVGTransformType::kMatrix:
      return "matrix(";
    case SVGTransformType::kTranslate:
      return "translate(";
    case SVGTransformType::kScale:
      return "scale(";
    case SVGTransformType::kRotate:
      return "rotate(";
    case SVGTransformType::kSkewx:
      return "skewX(";
    case SVGTransformType::kSkewy:
      return "skewY(";
  }
  NOTREACHED();
  return "";
}

}  // namespace

String SVGTransform::ValueAsString() const {
  double arguments[6];
  size_t argument_count = 0;
  switch (transform_type_) {
    case SVGTransformType::kUnknown:
      return g_empty_string;
    case SVGTransformType::kMatrix: {
      arguments[argument_count++] = matrix_.A();
      arguments[argument_count++] = matrix_.B();
      arguments[argument_count++] = matrix_.C();
      arguments[argument_count++] = matrix_.D();
      arguments[argument_count++] = matrix_.E();
      arguments[argument_count++] = matrix_.F();
      break;
    }
    case SVGTransformType::kTranslate: {
      arguments[argument_count++] = matrix_.E();
      arguments[argument_count++] = matrix_.F();
      break;
    }
    case SVGTransformType::kScale: {
      arguments[argument_count++] = matrix_.A();
      arguments[argument_count++] = matrix_.D();
      break;
    }
    case SVGTransformType::kRotate: {
      arguments[argument_count++] = angle_;

      double angle_in_rad = deg2rad(angle_);
      double cos_angle = cos(angle_in_rad);
      double sin_angle = sin(angle_in_rad);
      float cx = clampTo<float>(
          cos_angle != 1
              ? (matrix_.E() * (1 - cos_angle) - matrix_.F() * sin_angle) /
                    (1 - cos_angle) / 2
              : 0);
      float cy = clampTo<float>(
          cos_angle != 1
              ? (matrix_.E() * sin_angle / (1 - cos_angle) + matrix_.F()) / 2
              : 0);
      if (cx || cy) {
        arguments[argument_count++] = cx;
        arguments[argument_count++] = cy;
      }
      break;
    }
    case SVGTransformType::kSkewx:
      arguments[argument_count++] = angle_;
      break;
    case SVGTransformType::kSkewy:
      arguments[argument_count++] = angle_;
      break;
  }
  DCHECK_LE(argument_count, base::size(arguments));

  StringBuilder builder;
  builder.Append(TransformTypePrefixForParsing(transform_type_));

  for (size_t i = 0; i < argument_count; ++i) {
    if (i)
      builder.Append(' ');
    builder.AppendNumber(arguments[i]);
  }
  builder.Append(')');
  return builder.ToString();
}

void SVGTransform::Add(SVGPropertyBase*, SVGElement*) {
  // SVGTransform is not animated by itself.
  NOTREACHED();
}

void SVGTransform::CalculateAnimatedValue(const SVGAnimateElement&,
                                          float,
                                          unsigned,
                                          SVGPropertyBase*,
                                          SVGPropertyBase*,
                                          SVGPropertyBase*,
                                          SVGElement*) {
  // SVGTransform is not animated by itself.
  NOTREACHED();
}

float SVGTransform::CalculateDistance(SVGPropertyBase*, SVGElement*) {
  // SVGTransform is not animated by itself.
  NOTREACHED();

  return -1;
}

}  // namespace blink
