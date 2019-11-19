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

#include "third_party/blink/renderer/platform/transforms/perspective_transform_operation.h"

#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

scoped_refptr<TransformOperation> PerspectiveTransformOperation::Accumulate(
    const TransformOperation& other) {
  DCHECK(other.IsSameType(*this));
  double other_p = ToPerspectiveTransformOperation(other).p_;

  if (p_ == 0 && other_p == 0)
    return nullptr;

  // We want to solve:
  //   -1/p + -1/p' == -1/p'', where we know p and p'.
  //
  // This can be rewritten as:
  //   p'' == (p * p') / (p + p')
  double p = (p_ * other_p) / (p_ + other_p);
  return PerspectiveTransformOperation::Create(p);
}

scoped_refptr<TransformOperation> PerspectiveTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  if (from && !from->IsSameType(*this))
    return this;

  if (blend_to_identity) {
    // FIXME: this seems wrong.  https://bugs.webkit.org/show_bug.cgi?id=52700
    double p = blink::Blend(p_, 1., progress);
    return PerspectiveTransformOperation::Create(clampTo<int>(p, 0));
  }

  const PerspectiveTransformOperation* from_op =
      static_cast<const PerspectiveTransformOperation*>(from);

  TransformationMatrix from_t;
  TransformationMatrix to_t;
  from_t.ApplyPerspective(from_op ? from_op->p_ : 0);
  to_t.ApplyPerspective(p_);
  to_t.Blend(from_t, progress);

  TransformationMatrix::DecomposedType decomp;
  if (!to_t.Decompose(decomp)) {
    // If we can't decompose, bail out of interpolation.
    const PerspectiveTransformOperation* used_operation =
        progress > 0.5 ? this : from_op;
    return PerspectiveTransformOperation::Create(used_operation->Perspective());
  }

  if (decomp.perspective_z) {
    double val = -1.0 / decomp.perspective_z;
    return PerspectiveTransformOperation::Create(clampTo<double>(val, 0));
  }
  return PerspectiveTransformOperation::Create(0);
}

scoped_refptr<TransformOperation> PerspectiveTransformOperation::Zoom(
    double factor) {
  return Create(p_ * factor);
}

}  // namespace blink
