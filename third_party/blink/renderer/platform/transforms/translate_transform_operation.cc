/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"

#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

namespace {
Length AddLengths(const Length& lhs, const Length& rhs) {
  PixelsAndPercent lhs_pap = lhs.GetPixelsAndPercent();
  PixelsAndPercent rhs_pap = rhs.GetPixelsAndPercent();

  PixelsAndPercent result = lhs_pap + rhs_pap;
  if (result.percent == 0)
    return Length(result.pixels, Length::kFixed);
  if (result.pixels == 0)
    return Length(result.percent, Length::kPercent);
  return Length(CalculationValue::Create(result, Length::ValueRange::kAll));
}

TransformOperation::OperationType GetTypeForTranslate(const Length& x,
                                                      const Length& y,
                                                      double z) {
  bool x_zero = x.IsZero();
  bool y_zero = x.IsZero();
  bool z_zero = !z;
  if (y_zero && z_zero)
    return TransformOperation::kTranslateX;
  if (x_zero && z_zero)
    return TransformOperation::kTranslateY;
  if (x_zero && y_zero)
    return TransformOperation::kTranslateZ;
  if (z_zero)
    return TransformOperation::kTranslate;
  return TransformOperation::kTranslate3D;
}
}  // namespace

TransformOperation* TranslateTransformOperation::Accumulate(
    const TransformOperation& other) {
  DCHECK(other.CanBlendWith(*this));

  const auto& other_op = To<TranslateTransformOperation>(other);
  Length new_x = AddLengths(x_, other_op.x_);
  Length new_y = AddLengths(y_, other_op.y_);
  double new_z = z_ + other_op.z_;
  return MakeGarbageCollected<TranslateTransformOperation>(
      new_x, new_y, new_z, GetTypeForTranslate(new_x, new_y, new_z));
}

TransformOperation* TranslateTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  const Length zero_length = Length::Fixed(0);
  if (blend_to_identity) {
    return MakeGarbageCollected<TranslateTransformOperation>(
        zero_length.Blend(x_, progress, Length::ValueRange::kAll),
        zero_length.Blend(y_, progress, Length::ValueRange::kAll),
        blink::Blend(z_, 0., progress), type_);
  }

  const auto* from_op = To<TranslateTransformOperation>(from);
  const Length& from_x = from_op ? from_op->x_ : zero_length;
  const Length& from_y = from_op ? from_op->y_ : zero_length;
  double from_z = from_op ? from_op->z_ : 0;
  OperationType type;

  CommonPrimitiveForInterpolation(from, type);

  return MakeGarbageCollected<TranslateTransformOperation>(
      x_.Blend(from_x, progress, Length::ValueRange::kAll),
      y_.Blend(from_y, progress, Length::ValueRange::kAll),
      blink::Blend(from_z, z_, progress), type);
}

TranslateTransformOperation* TranslateTransformOperation::ZoomTranslate(
    double factor) {
  return MakeGarbageCollected<TranslateTransformOperation>(
      x_.Zoom(factor), y_.Zoom(factor), z_ * factor, type_);
}

void TranslateTransformOperation::CommonPrimitiveForInterpolation(
    const TransformOperation* from,
    TransformOperation::OperationType& common_type) const {
  bool is_3d = Is3DOperation() || (from && from->Is3DOperation());
  const auto* from_op = To<TranslateTransformOperation>(from);
  TransformOperation::OperationType from_type =
      from_op ? from_op->type_ : type_;
  if (type_ == from_type) {
    common_type = type_;
  } else if (is_3d) {
    common_type = kTranslate3D;
  } else {
    common_type = kTranslate;
  }
}

}  // namespace blink
