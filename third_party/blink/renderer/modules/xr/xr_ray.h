// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RAY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RAY_H_

#include <memory>

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class DOMPointInit;
class DOMPointReadOnly;
class ExceptionState;
class XRRigidTransform;

class XRRay final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRRay();
  explicit XRRay(const TransformationMatrix& matrix,
                 ExceptionState& exception_state);
  explicit XRRay(XRRigidTransform* transform, ExceptionState& exception_state);
  XRRay(DOMPointInit* origin,
        DOMPointInit* direction,
        ExceptionState& exception_state);
  ~XRRay() override;

  DOMPointReadOnly* origin() const { return origin_; }
  DOMPointReadOnly* direction() const { return direction_; }
  DOMFloat32Array* matrix();

  // Calling |RawMatrix()| is equivalent to calling |matrix()| w.r.t. the data
  // that will be returned, the only difference is the returned type.
  TransformationMatrix RawMatrix();

  static XRRay* Create(ExceptionState& exception_state);
  static XRRay* Create(DOMPointInit* origin, ExceptionState& exception_state);
  static XRRay* Create(DOMPointInit* origin,
                       DOMPointInit* direction,
                       ExceptionState& exception_state);
  static XRRay* Create(XRRigidTransform* transform,
                       ExceptionState& exception_state);

  void Trace(blink::Visitor*) override;

 private:
  void Set(const TransformationMatrix& matrix, ExceptionState& exception_state);
  void Set(FloatPoint3D origin,
           FloatPoint3D direction,
           ExceptionState& exception_state);

  Member<DOMPointReadOnly> origin_;
  Member<DOMPointReadOnly> direction_;
  Member<DOMFloat32Array> matrix_;
  std::unique_ptr<TransformationMatrix> raw_matrix_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_RAY_H_
