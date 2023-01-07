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

#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"

#include <algorithm>

namespace blink {

scoped_refptr<TransformOperation> MatrixTransformOperation::Accumulate(
    const TransformOperation& other_op) {
  DCHECK(other_op.IsSameType(*this));
  const auto& other = To<MatrixTransformOperation>(other_op);

  // Similar to interpolation, accumulating matrices is done by decomposing
  // them, accumulating the individual functions, and then recomposing.

  TransformationMatrix::Decomposed2dType from_decomp;
  TransformationMatrix::Decomposed2dType to_decomp;
  if (!other.matrix_.Decompose2D(from_decomp) ||
      !matrix_.Decompose2D(to_decomp)) {
    return nullptr;
  }

  // For a 2D matrix, the components can just be naively summed, noting that
  // scale uses 1-based addition.
  from_decomp.scale_x += to_decomp.scale_x - 1;
  from_decomp.scale_y += to_decomp.scale_y - 1;
  from_decomp.skew_xy += to_decomp.skew_xy;
  from_decomp.translate_x += to_decomp.translate_x;
  from_decomp.translate_y += to_decomp.translate_y;
  from_decomp.angle += to_decomp.angle;

  TransformationMatrix result;
  result.Recompose2D(from_decomp);
  return MatrixTransformOperation::Create(result);
}

scoped_refptr<TransformOperation> MatrixTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  // convert the TransformOperations into matrices
  if (!matrix_.IsInvertible())
    return nullptr;

  TransformationMatrix from_t;
  if (from) {
    const MatrixTransformOperation* m =
        static_cast<const MatrixTransformOperation*>(from);
    from_t = m->matrix_;
    if (!from_t.IsInvertible())
      return nullptr;
  }

  TransformationMatrix to_t = matrix_;
  if (blend_to_identity)
    std::swap(from_t, to_t);

  to_t.Blend(from_t, progress);
  return MatrixTransformOperation::Create(to_t);
}

scoped_refptr<TransformOperation> MatrixTransformOperation::Zoom(
    double factor) {
  TransformationMatrix m = matrix_;
  m.Zoom(factor);
  return Create(m);
}

}  // namespace blink
