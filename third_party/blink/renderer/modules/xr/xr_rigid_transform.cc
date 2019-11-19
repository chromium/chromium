// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"

#include <utility>

#include "third_party/blink/renderer/core/geometry/dom_point_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

// makes a deep copy of transformationMatrix
XRRigidTransform::XRRigidTransform(
    const TransformationMatrix& transformationMatrix)
    : matrix_(std::make_unique<TransformationMatrix>(transformationMatrix)) {
  DecomposeMatrix();
}

void XRRigidTransform::DecomposeMatrix() {
  // decompose matrix to position and orientation
  TransformationMatrix::DecomposedType decomposed;
  bool succeeded = matrix_->Decompose(decomposed);
  DCHECK(succeeded) << "Matrix decompose failed for " << matrix_->ToString();

  position_ =
      DOMPointReadOnly::Create(decomposed.translate_x, decomposed.translate_y,
                               decomposed.translate_z, 1.0);

  // TODO(https://crbug.com/929841): Minuses are needed as a workaround for
  // bug in TransformationMatrix so that callers can still pass non-inverted
  // quaternions.
  orientation_ = makeNormalizedQuaternion(
      -decomposed.quaternion_x, -decomposed.quaternion_y,
      -decomposed.quaternion_z, decomposed.quaternion_w);
}

XRRigidTransform::XRRigidTransform(DOMPointInit* position,
                                   DOMPointInit* orientation) {
  if (position) {
    position_ = DOMPointReadOnly::Create(position->x(), position->y(),
                                         position->z(), 1.0);
  } else {
    position_ = DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  }

  if (orientation) {
    orientation_ = makeNormalizedQuaternion(orientation->x(), orientation->y(),
                                            orientation->z(), orientation->w());
  } else {
    orientation_ = DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  }

  // Computing transformation matrix from position and orientation is expensive,
  // so compute it lazily in matrix().
}

XRRigidTransform* XRRigidTransform::Create(DOMPointInit* position,
                                           DOMPointInit* orientation,
                                           ExceptionState& exception_state) {
  if (position && position->w() != 1.0) {
    exception_state.ThrowTypeError("W component of position must be 1.0");
    return nullptr;
  }

  if (orientation) {
    double x = orientation->x();
    double y = orientation->y();
    double z = orientation->z();
    double w = orientation->w();
    double sq_len = x * x + y * y + z * z + w * w;

    // The only way for the result of a square root to be 0 is if the squared
    // number is 0, so save the square root operation and just compare to 0 now.
    if (sq_len == 0.0) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Orientation's length cannot be 0");
      return nullptr;
    }
  }

  return MakeGarbageCollected<XRRigidTransform>(position, orientation);
}

DOMFloat32Array* XRRigidTransform::matrix() {
  EnsureMatrix();
  if (!matrix_array_) {
    matrix_array_ = transformationMatrixToDOMFloat32Array(*matrix_);
  }

  if (!matrix_array_ || !matrix_array_->View() ||
      !matrix_array_->View()->Data()) {
    // A page may take the matrix_array_ value and detach it so matrix_array_ is
    // a detached array buffer.  This breaks the inspector, so return null
    // instead.
    return nullptr;
  }

  return matrix_array_;
}

XRRigidTransform* XRRigidTransform::inverse() {
  EnsureInverse();
  return inverse_;
}

TransformationMatrix XRRigidTransform::InverseTransformMatrix() {
  EnsureInverse();
  return inverse_->TransformMatrix();
}

TransformationMatrix XRRigidTransform::TransformMatrix() {
  EnsureMatrix();
  return *matrix_;
}

void XRRigidTransform::EnsureMatrix() {
  if (!matrix_) {
    matrix_ = std::make_unique<TransformationMatrix>();
    TransformationMatrix::DecomposedType decomp;
    memset(&decomp, 0, sizeof(decomp));
    decomp.perspective_w = 1;
    decomp.scale_x = 1;
    decomp.scale_y = 1;
    decomp.scale_z = 1;

    // TODO(https://crbug.com/929841): Minuses are needed as a workaround for
    // bug in TransformationMatrix so that callers can still pass non-inverted
    // quaternions.
    decomp.quaternion_x = -orientation_->x();
    decomp.quaternion_y = -orientation_->y();
    decomp.quaternion_z = -orientation_->z();
    decomp.quaternion_w = orientation_->w();

    decomp.translate_x = position_->x();
    decomp.translate_y = position_->y();
    decomp.translate_z = position_->z();

    matrix_->Recompose(decomp);
  }
}

void XRRigidTransform::EnsureInverse() {
  // Only compute inverse matrix when it's requested, but cache it once we do.
  // matrix_ does not change once the XRRigidTransfrorm has been constructed, so
  // the caching is safe.
  if (!inverse_) {
    EnsureMatrix();
    DCHECK(matrix_->IsInvertible());
    inverse_ = MakeGarbageCollected<XRRigidTransform>(matrix_->Inverse());
    inverse_->inverse_ = this;
  }
}

void XRRigidTransform::Trace(blink::Visitor* visitor) {
  visitor->Trace(position_);
  visitor->Trace(orientation_);
  visitor->Trace(inverse_);
  visitor->Trace(matrix_array_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
