/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"

#include "third_party/blink/renderer/platform/transforms/rotation.h"
#include "ui/gfx/geometry/quaternion.h"

#include <algorithm>

namespace blink {

scoped_refptr<TransformOperation> Matrix3DTransformOperation::Accumulate(
    const TransformOperation& other_op) {
  DCHECK(other_op.IsSameType(*this));
  const auto& other = ToMatrix3DTransformOperation(other_op);

  // If either matrix is non-invertible, fail and fallback to replace.
  if (!matrix_.IsInvertible() || !other.matrix_.IsInvertible())
    return nullptr;

  // Similar to interpolation, accumulating 3D matrices is done by decomposing
  // them, accumulating the individual functions, and then recomposing.

  TransformationMatrix::DecomposedType from_decomp;
  TransformationMatrix::DecomposedType to_decomp;
  if (!matrix_.Decompose(from_decomp) || !other.matrix_.Decompose(to_decomp))
    return nullptr;

  // Scale is accumulated using 1-based addition.
  from_decomp.scale_x += to_decomp.scale_x - 1;
  from_decomp.scale_y += to_decomp.scale_y - 1;
  from_decomp.scale_z += to_decomp.scale_z - 1;

  // Skew can be added.
  from_decomp.skew_xy += to_decomp.skew_xy;
  from_decomp.skew_xz += to_decomp.skew_xz;
  from_decomp.skew_yz += to_decomp.skew_yz;

  // To accumulate quaternions, we multiply them. This is equivalent to 'adding'
  // the rotations that they represent.
  gfx::Quaternion from_quaternion(
      from_decomp.quaternion_x, from_decomp.quaternion_y,
      from_decomp.quaternion_z, from_decomp.quaternion_w);
  gfx::Quaternion to_quaternion(to_decomp.quaternion_x, to_decomp.quaternion_y,
                                to_decomp.quaternion_z, to_decomp.quaternion_w);

  gfx::Quaternion result_quaternion = from_quaternion * to_quaternion;
  from_decomp.quaternion_x = result_quaternion.x();
  from_decomp.quaternion_y = result_quaternion.y();
  from_decomp.quaternion_z = result_quaternion.z();
  from_decomp.quaternion_w = result_quaternion.w();

  // Translate is a simple addition.
  from_decomp.translate_x += to_decomp.translate_x;
  from_decomp.translate_y += to_decomp.translate_y;
  from_decomp.translate_z += to_decomp.translate_z;

  // We sum the perspective components; note that w is 1-based.
  from_decomp.perspective_x += to_decomp.perspective_x;
  from_decomp.perspective_y += to_decomp.perspective_y;
  from_decomp.perspective_z += to_decomp.perspective_z;
  from_decomp.perspective_w += to_decomp.perspective_w - 1;

  TransformationMatrix result;
  result.Recompose(from_decomp);
  return Matrix3DTransformOperation::Create(result);
}

scoped_refptr<TransformOperation> Matrix3DTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  if (from && !from->IsSameType(*this))
    return this;

  // Convert the TransformOperations into matrices. Fail the blend operation
  // if either of the matrices is non-invertible.
  FloatSize size;
  TransformationMatrix from_t;
  TransformationMatrix to_t;
  if (from) {
    from->Apply(from_t, size);
    if (!from_t.IsInvertible())
      return nullptr;
  }

  Apply(to_t, size);
  if (!to_t.IsInvertible())
    return nullptr;

  if (blend_to_identity)
    std::swap(from_t, to_t);

  to_t.Blend(from_t, progress);
  return Matrix3DTransformOperation::Create(to_t);
}

scoped_refptr<TransformOperation> Matrix3DTransformOperation::Zoom(
    double factor) {
  TransformationMatrix result = matrix_;
  result.Zoom(factor);
  return Create(result);
}

}  // namespace blink
