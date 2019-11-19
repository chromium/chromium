/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/svg_transform_tear_off.h"

#include "third_party/blink/renderer/core/svg/svg_matrix_tear_off.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGTransformTearOff::SVGTransformTearOff(SVGMatrixTearOff* matrix)
    : SVGTransformTearOff(MakeGarbageCollected<SVGTransform>(matrix->Value()),
                          nullptr,
                          kPropertyIsNotAnimVal) {}

SVGTransformTearOff::SVGTransformTearOff(
    SVGTransform* target,
    SVGAnimatedPropertyBase* binding,
    PropertyIsAnimValType property_is_anim_val)
    : SVGPropertyTearOff<SVGTransform>(target, binding, property_is_anim_val) {}

SVGTransformTearOff::~SVGTransformTearOff() = default;

void SVGTransformTearOff::Trace(blink::Visitor* visitor) {
  visitor->Trace(matrix_tearoff_);
  SVGPropertyTearOff<SVGTransform>::Trace(visitor);
}

SVGTransformTearOff* SVGTransformTearOff::CreateDetached() {
  return MakeGarbageCollected<SVGTransformTearOff>(
      MakeGarbageCollected<SVGTransform>(blink::SVGTransformType::kMatrix),
      nullptr, kPropertyIsNotAnimVal);
}

SVGMatrixTearOff* SVGTransformTearOff::matrix() {
  if (!matrix_tearoff_)
    matrix_tearoff_ = MakeGarbageCollected<SVGMatrixTearOff>(this);
  return matrix_tearoff_.Get();
}

void SVGTransformTearOff::setMatrix(SVGMatrixTearOff* matrix,
                                    ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetMatrix(matrix->Value());
  CommitChange();
}

void SVGTransformTearOff::setTranslate(float tx,
                                       float ty,
                                       ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetTranslate(tx, ty);
  CommitChange();
}

void SVGTransformTearOff::setScale(float sx,
                                   float sy,
                                   ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetScale(sx, sy);
  CommitChange();
}

void SVGTransformTearOff::setRotate(float angle,
                                    float cx,
                                    float cy,
                                    ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetRotate(angle, cx, cy);
  CommitChange();
}

void SVGTransformTearOff::setSkewX(float x, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetSkewX(x);
  CommitChange();
}

void SVGTransformTearOff::setSkewY(float y, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetSkewY(y);
  CommitChange();
}

}  // namespace blink
