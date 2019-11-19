// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_ray.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "third_party/blink/renderer/core/geometry/dom_point_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

XRRay::XRRay() {
  origin_ = DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  direction_ = DOMPointReadOnly::Create(0.0, 0.0, -1.0, 0.0);
}

XRRay::XRRay(const TransformationMatrix& matrix,
             ExceptionState& exception_state) {
  Set(matrix, exception_state);
}

XRRay::XRRay(XRRigidTransform* transform, ExceptionState& exception_state) {
  DOMFloat32Array* m = transform->matrix();
  Set(DOMFloat32ArrayToTransformationMatrix(m), exception_state);
}

XRRay::XRRay(DOMPointInit* origin,
             DOMPointInit* direction,
             ExceptionState& exception_state) {
  FloatPoint3D o;
  if (origin) {
    o = FloatPoint3D(origin->x(), origin->y(), origin->z());
  } else {
    o = FloatPoint3D(0.f, 0.f, 0.f);
  }

  FloatPoint3D d;
  if (direction) {
    d = FloatPoint3D(direction->x(), direction->y(), direction->z());
  } else {
    d = FloatPoint3D(0.f, 0.f, -1.f);
  }

  Set(o, d, exception_state);
}

void XRRay::Set(const TransformationMatrix& matrix,
                ExceptionState& exception_state) {
  FloatPoint3D origin = matrix.MapPoint(FloatPoint3D(0, 0, 0));
  FloatPoint3D direction = matrix.MapPoint(FloatPoint3D(0, 0, -1));
  direction.Move(-origin.X(), -origin.Y(), -origin.Z());

  Set(origin, direction, exception_state);
}

// Sets member variables from passed in |origin| and |direction|.
// All constructors with the exception of default constructor eventually invoke
// this method.
// If the |direction|'s length is 0, this method will initialize direction to
// default vector (0, 0, -1).
void XRRay::Set(FloatPoint3D origin,
                FloatPoint3D direction,
                ExceptionState& exception_state) {
  DVLOG(3) << __FUNCTION__ << ": origin=" << origin.ToString()
           << ", direction=" << direction.ToString();

  if (direction.length() == 0.0f) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kUnableToNormalizeZeroLength);
    return;
  } else {
    direction.Normalize();
  }

  origin_ = DOMPointReadOnly::Create(origin.X(), origin.Y(), origin.Z(), 1.0);
  direction_ = DOMPointReadOnly::Create(direction.X(), direction.Y(),
                                        direction.Z(), 0.0);
}

XRRay* XRRay::Create(XRRigidTransform* transform,
                     ExceptionState& exception_state) {
  auto* result = MakeGarbageCollected<XRRay>(transform, exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  return result;
}

XRRay* XRRay::Create(ExceptionState& exception_state) {
  auto* result = MakeGarbageCollected<XRRay>(nullptr, nullptr, exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  return result;
}

XRRay* XRRay::Create(DOMPointInit* origin, ExceptionState& exception_state) {
  auto* result = MakeGarbageCollected<XRRay>(origin, nullptr, exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  return result;
}

XRRay* XRRay::Create(DOMPointInit* origin,
                     DOMPointInit* direction,
                     ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<XRRay>(origin, direction, exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  return result;
}

XRRay::~XRRay() {}

DOMFloat32Array* XRRay::matrix() {
  DVLOG(3) << __FUNCTION__;

  // A page may take the matrix value and detach it so matrix_ is a detached
  // array buffer.  If that's the case, recompute the matrix.
  // Step 1. If transform’s internal matrix is not null, perform the following
  // steps:
  //    Step 1. If the operation IsDetachedBuffer on internal matrix is false,
  //    return transform’s internal matrix.
  if (!matrix_ || !matrix_->View() || !matrix_->View()->Data()) {
    // Returned matrix should represent transformation from ray originating at
    // (0,0,0) with direction (0,0,-1) into ray originating at |origin_| with
    // direction |direction_|.

    TransformationMatrix matrix;

    const blink::FloatPoint3D desiredRayDirection = {
        direction_->x(), direction_->y(), direction_->z()};

    // Translation from 0 to |origin_| is simply translation by |origin_|.
    // (implicit) Step 6: Let translation be the translation matrix with
    // components corresponding to ray’s origin
    matrix.Translate3d(origin_->x(), origin_->y(), origin_->z());

    // Step 2: Let z be the vector [0, 0, -1]
    const blink::FloatPoint3D initialRayDirection =
        blink::FloatPoint3D{0.f, 0.f, -1.f};

    // Step 3: Let axis be the vector cross product of z and ray’s direction,
    // z × direction
    blink::FloatPoint3D axis = initialRayDirection.Cross(desiredRayDirection);

    // Step 4: Let cos_angle be the scalar dot product of z and ray’s direction,
    // z · direction
    float cos_angle = initialRayDirection.Dot(desiredRayDirection);

    // Step 5: Set rotation based on the following:
    if (cos_angle > 0.9999) {
      // Vectors are co-linear or almost co-linear & face the same direction,
      // no rotation is needed.

    } else if (cos_angle < -0.9999) {
      // Vectors are co-linear or almost co-linear & face the opposite
      // direction, rotation by 180 degrees is needed & can be around any vector
      // perpendicular to (0,0,-1) so let's rotate by (1, 0, 0).
      matrix.Rotate3d(1, 0, 0, 180);
    } else {
      // Rotation needed - create it from axis-angle.
      matrix.Rotate3d(axis.X(), axis.Y(), axis.Z(),
                      rad2deg(std::acos(cos_angle)));
    }

    // Step 7: Let matrix be the result of premultiplying rotation from the left
    // onto translation (i.e. translation * rotation) in column-vector notation.
    // Step 8: Set ray’s internal matrix to matrix
    matrix_ = transformationMatrixToDOMFloat32Array(matrix);
    if (!raw_matrix_) {
      raw_matrix_ = std::make_unique<TransformationMatrix>(matrix);
    } else {
      *raw_matrix_ = matrix;
    }
  }

  // Step 9: Return matrix
  return matrix_;
}

TransformationMatrix XRRay::RawMatrix() {
  matrix();

  DCHECK(raw_matrix_);

  return *raw_matrix_;
}

void XRRay::Trace(blink::Visitor* visitor) {
  visitor->Trace(origin_);
  visitor->Trace(direction_);
  visitor->Trace(matrix_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
