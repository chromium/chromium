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
  const MatrixTransformOperation& other = ToMatrixTransformOperation(other_op);

  TransformationMatrix from_t(other.a_, other.b_, other.c_, other.d_, other.e_,
                              other.f_);
  TransformationMatrix to_t(a_, b_, c_, d_, e_, f_);

  // If either matrix is non-invertible, fail and fallback to replace.
  if (!from_t.IsInvertible() || !to_t.IsInvertible())
    return nullptr;

  // Similar to interpolation, accumulating matrices is done by decomposing
  // them, accumulating the individual functions, and then recomposing.

  TransformationMatrix::Decomposed2dType from_decomp;
  TransformationMatrix::Decomposed2dType to_decomp;
  if (!from_t.Decompose2D(from_decomp) || !to_t.Decompose2D(to_decomp)) {
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

  from_t.Recompose2D(from_decomp);
  return MatrixTransformOperation::Create(from_t);
}

scoped_refptr<TransformOperation> MatrixTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  if (from && !from->IsSameType(*this))
    return this;

  // convert the TransformOperations into matrices
  TransformationMatrix from_t;
  TransformationMatrix to_t(a_, b_, c_, d_, e_, f_);
  if (!to_t.IsInvertible())
    return nullptr;
  if (from) {
    const MatrixTransformOperation* m =
        static_cast<const MatrixTransformOperation*>(from);
    from_t.SetMatrix(m->a_, m->b_, m->c_, m->d_, m->e_, m->f_);
    if (!from_t.IsInvertible())
      return nullptr;
  }

  if (blend_to_identity)
    std::swap(from_t, to_t);

  to_t.Blend(from_t, progress);
  return MatrixTransformOperation::Create(to_t.A(), to_t.B(), to_t.C(),
                                          to_t.D(), to_t.E(), to_t.F());
}

scoped_refptr<TransformOperation> MatrixTransformOperation::Zoom(
    double factor) {
  return Create(a_, b_, c_, d_, e_ * factor, f_ * factor);
}

}  // namespace blink
