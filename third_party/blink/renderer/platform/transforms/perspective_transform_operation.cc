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

#include <algorithm>
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

scoped_refptr<TransformOperation> PerspectiveTransformOperation::Accumulate(
    const TransformOperation& other) {
  DCHECK(other.IsSameType(*this));
  double other_p = To<PerspectiveTransformOperation>(other).UsedPerspective();
  double p = UsedPerspective();

  // We want to solve:
  //   -1/p + -1/p' == -1/p'', where we know p and p'.
  //
  // This can be rewritten as:
  //   p'' == (p * p') / (p + p')
  return PerspectiveTransformOperation::Create((p * other_p) / (p + other_p));
}

scoped_refptr<TransformOperation> PerspectiveTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  if (from && !from->IsSameType(*this))
    return this;

  // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
  // says that we should run matrix decomposition and then run the rules for
  // interpolation of matrices, but we know what those rules are going to
  // yield, so just do that directly.
  double from_p_inverse, to_p_inverse;
  if (blend_to_identity) {
    from_p_inverse = 1.0 / UsedPerspective();
    to_p_inverse = 0.0;
  } else {
    if (from) {
      const PerspectiveTransformOperation* from_op =
          static_cast<const PerspectiveTransformOperation*>(from);
      from_p_inverse = 1.0 / from_op->UsedPerspective();
    } else {
      from_p_inverse = 0.0;
    }
    to_p_inverse = 1.0 / UsedPerspective();
  }
  double p =
      1.0 / std::max(0.0, blink::Blend(from_p_inverse, to_p_inverse, progress));
  return PerspectiveTransformOperation::Create(clampTo<double>(p, 0));
}

scoped_refptr<TransformOperation> PerspectiveTransformOperation::Zoom(
    double factor) {
  return Create(p_ * factor);
}

}  // namespace blink
