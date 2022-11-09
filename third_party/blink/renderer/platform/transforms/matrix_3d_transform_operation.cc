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
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/quaternion.h"

#include <algorithm>

namespace blink {

scoped_refptr<TransformOperation> Matrix3DTransformOperation::Accumulate(
    const TransformOperation& other_op) {
  DCHECK(other_op.IsSameType(*this));
  const auto& other = To<Matrix3DTransformOperation>(other_op);

  // If either matrix is non-invertible, fail and fallback to replace.
  if (!matrix_.IsInvertible() || !other.matrix_.IsInvertible())
    return nullptr;

  // Similar to interpolation, accumulating 3D matrices is done by decomposing
  // them, accumulating the individual functions, and then recomposing.

  absl::optional<gfx::DecomposedTransform> from_decomp = matrix_.Decompose();
  if (!from_decomp)
    return nullptr;

  absl::optional<gfx::DecomposedTransform> to_decomp =
      other.matrix_.Decompose();
  if (!to_decomp)
    return nullptr;

  // Scale is accumulated using 1-based addition.
  for (size_t i = 0; i < std::size(from_decomp->scale); i++)
    from_decomp->scale[i] += to_decomp->scale[i] - 1;

  // Skew can be added.
  for (size_t i = 0; i < std::size(from_decomp->skew); i++)
    from_decomp->skew[i] += to_decomp->skew[i];

  // To accumulate quaternions, we multiply them. This is equivalent to 'adding'
  // the rotations that they represent.
  from_decomp->quaternion = from_decomp->quaternion * to_decomp->quaternion;

  // Translate is a simple addition.
  for (size_t i = 0; i < std::size(from_decomp->translate); i++)
    from_decomp->translate[i] += to_decomp->translate[i];

  // We sum the perspective components; note that w is 1-based.
  for (size_t i = 0; i < std::size(from_decomp->perspective) - 1; i++)
    from_decomp->perspective[i] += to_decomp->perspective[i];
  from_decomp->perspective[3] += to_decomp->perspective[3] - 1;

  TransformationMatrix result = gfx::Transform::Compose(*from_decomp);
  return Matrix3DTransformOperation::Create(result);
}

scoped_refptr<TransformOperation> Matrix3DTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  // Convert the TransformOperations into matrices. Fail the blend operation
  // if either of the matrices is non-invertible.
  gfx::SizeF size;
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
