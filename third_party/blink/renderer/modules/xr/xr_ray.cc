// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_ray.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_ray_direction_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace {

constexpr char kInvalidWComponentInOrigin[] =
    "Origin's `w` component must be set to 1.0f!";
constexpr char kInvalidWComponentInDirection[] =
    "Direction's `w` component must be set to 0.0f!";

}  // namespace

namespace blink {

XRRay::XRRay() {
  origin_ = DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  direction_ = DOMPointReadOnly::Create(0.0, 0.0, -1.0, 0.0);
}

XRRay::XRRay(XRRigidTransform* transform, ExceptionState& exception_state) {
  DOMFloat32Array* m = transform->matrix();
  Set(DOMFloat32ArrayToTransform(m), exception_state);
}

XRRay::XRRay(DOMPointInit* origin,
             XRRayDirectionInit* direction,
             ExceptionState& exception_state) {
  DCHECK(origin);
  DCHECK(direction);

  gfx::Point3F o(origin->x(), origin->y(), origin->z());
  gfx::Vector3dF d(direction->x(), direction->y(), direction->z());

  if (d.LengthSquared() == 0.0f) {
    exception_state.ThrowTypeError(kUnableToNormalizeZeroLength);
    return;
  }

  if (direction->w() != 0.0f) {
    exception_state.ThrowTypeError(kInvalidWComponentInDirection);
    return;
  }

  if (origin->w() != 1.0f) {
    exception_state.ThrowTypeError(kInvalidWComponentInOrigin);
    return;
  }

  Set(o, d, exception_state);
}

void XRRay::Set(const gfx::Transform& matrix, ExceptionState& exception_state) {
  gfx::Point3F origin = matrix.MapPoint(gfx::Point3F(0, 0, 0));
  gfx::Point3F direction_point = matrix.MapPoint(gfx::Point3F(0, 0, -1));
  Set(origin, direction_point - origin, exception_state);
}

// Sets member variables from passed in |origin| and |direction|.
// All constructors with the exception of default constructor eventually invoke
// this method.
// If the |direction|'s length is 0, this method will initialize direction to
// default vector (0, 0, -1).
void XRRay::Set(gfx::Point3F origin,
                gfx::Vector3dF direction,
                ExceptionState& exception_state) {
  DVLOG(3) << __FUNCTION__ << ": origin=" << origin.ToString()
           << ", direction=" << direction.ToString();

  gfx::Vector3dF normalized_direction;
  if (!direction.GetNormalized(&normalized_direction))
    normalized_direction = gfx::Vector3dF(0, 0, -1);

  origin_ = DOMPointReadOnly::Create(origin.x(), origin.y(), origin.z(), 1.0);
  direction_ = DOMPointReadOnly::Create(normalized_direction.x(),
                                        normalized_direction.y(),
                                        normalized_direction.z(), 0.0);
}

XRRay* XRRay::Create(XRRigidTransform* transform,
                     ExceptionState& exception_state) {
  auto* result = MakeGarbageCollected<XRRay>(transform, exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  return result;
}

XRRay* XRRay::Create(DOMPointInit* origin,
                     XRRayDirectionInit* direction,
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
  if (!matrix_ || !matrix_->Data()) {
    // Returned matrix should represent transformation from ray originating at
    // (0,0,0) with direction (0,0,-1) into ray originating at |origin_| with
    // direction |direction_|.

    gfx::Transform matrix;

    const gfx::Vector3dF desired_ray_direction(
        static_cast<float>(direction_->x()),
        static_cast<float>(direction_->y()),
        static_cast<float>(direction_->z()));

    // Translation from 0 to |origin_| is simply translation by |origin_|.
    // (implicit) Step 6: Let translation be the translation matrix with
    // components corresponding to ray’s origin
    matrix.Translate3d(origin_->x(), origin_->y(), origin_->z());

    // Step 2: Let z be the vector [0, 0, -1]
    const gfx::Vector3dF initial_ray_direction(0.f, 0.f, -1.f);

    // Step 3: Let axis be the vector cross product of z and ray’s direction,
    // z × direction
    gfx::Vector3dF axis =
        gfx::CrossProduct(initial_ray_direction, desired_ray_direction);

    // Step 4: Let cos_angle be the scalar dot product of z and ray’s direction,
    // z · direction
    float cos_angle =
        gfx::DotProduct(initial_ray_direction, desired_ray_direction);

    // Step 5: Set rotation based on the following:
    if (cos_angle > 0.9999) {
      // Vectors are co-linear or almost co-linear & face the same direction,
      // no rotation is needed.

    } else if (cos_angle < -0.9999) {
      // Vectors are co-linear or almost co-linear & face the opposite
      // direction, rotation by 180 degrees is needed & can be around any vector
      // perpendicular to (0,0,-1) so let's rotate about the x-axis.
      matrix.RotateAboutXAxis(180);
    } else {
      // Rotation needed - create it from axis-angle.
      matrix.RotateAbout(axis, Rad2deg(std::acos(cos_angle)));
    }

    // Step 7: Let matrix be the result of premultiplying rotation from the left
    // onto translation (i.e. translation * rotation) in column-vector notation.
    // Step 8: Set ray’s internal matrix to matrix
    matrix_ = transformationMatrixToDOMFloat32Array(matrix);
    if (!raw_matrix_) {
      raw_matrix_ = std::make_unique<gfx::Transform>(matrix);
    } else {
      *raw_matrix_ = matrix;
    }
  }

  // Step 9: Return matrix
  return matrix_.Get();
}

gfx::Transform XRRay::RawMatrix() {
  matrix();

  DCHECK(raw_matrix_);

  return *raw_matrix_;
}

void XRRay::Trace(Visitor* visitor) const {
  visitor->Trace(origin_);
  visitor->Trace(direction_);
  visitor->Trace(matrix_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
