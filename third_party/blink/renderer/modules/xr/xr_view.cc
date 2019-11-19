// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_view.h"

#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"

namespace blink {

XRView::XRView(XRSession* session, const XRViewData& view_data)
    : eye_(view_data.Eye()), session_(session) {
  switch (eye_) {
    case kEyeLeft:
      eye_string_ = "left";
      break;
    case kEyeRight:
      eye_string_ = "right";
      break;
    default:
      eye_string_ = "none";
  }
  ref_space_from_eye_ =
      MakeGarbageCollected<XRRigidTransform>(view_data.Transform());
  projection_matrix_ =
      transformationMatrixToDOMFloat32Array(view_data.ProjectionMatrix());
}

XRSession* XRView::session() const {
  return session_;
}

DOMFloat32Array* XRView::projectionMatrix() const {
  if (!projection_matrix_ || !projection_matrix_->View() ||
      !projection_matrix_->View()->Data()) {
    // A page may take the projection matrix value and detach it so
    // projection_matrix_ is a detached array buffer.  This breaks the
    // inspector, so return null instead.
    return nullptr;
  }

  return projection_matrix_;
}

void XRViewData::UpdateProjectionMatrixFromFoV(float up_rad,
                                               float down_rad,
                                               float left_rad,
                                               float right_rad,
                                               float near_depth,
                                               float far_depth) {
  float up_tan = tanf(up_rad);
  float down_tan = tanf(down_rad);
  float left_tan = tanf(left_rad);
  float right_tan = tanf(right_rad);
  float x_scale = 2.0f / (left_tan + right_tan);
  float y_scale = 2.0f / (up_tan + down_tan);
  float inv_nf = 1.0f / (near_depth - far_depth);

  projection_matrix_ = TransformationMatrix(
      x_scale, 0.0f, 0.0f, 0.0f, 0.0f, y_scale, 0.0f, 0.0f,
      -((left_tan - right_tan) * x_scale * 0.5),
      ((up_tan - down_tan) * y_scale * 0.5), (near_depth + far_depth) * inv_nf,
      -1.0f, 0.0f, 0.0f, (2.0f * far_depth * near_depth) * inv_nf, 0.0f);
}

void XRViewData::UpdateProjectionMatrixFromAspect(float fovy,
                                                  float aspect,
                                                  float near_depth,
                                                  float far_depth) {
  float f = 1.0f / tanf(fovy / 2);
  float inv_nf = 1.0f / (near_depth - far_depth);

  projection_matrix_ = TransformationMatrix(
      f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f,
      (far_depth + near_depth) * inv_nf, -1.0f, 0.0f, 0.0f,
      (2.0f * far_depth * near_depth) * inv_nf, 0.0f);

  inv_projection_dirty_ = true;
}

TransformationMatrix XRViewData::UnprojectPointer(double x,
                                                  double y,
                                                  double canvas_width,
                                                  double canvas_height) {
  // Recompute the inverse projection matrix if needed.
  if (inv_projection_dirty_) {
    inv_projection_ = projection_matrix_.Inverse();
    inv_projection_dirty_ = false;
  }

  // Transform the x/y coordinate into WebGL normalized device coordinates.
  // Z coordinate of -1 means the point will be projected onto the projection
  // matrix near plane.
  FloatPoint3D point_in_projection_space(
      x / canvas_width * 2.0 - 1.0,
      (canvas_height - y) / canvas_height * 2.0 - 1.0, -1.0);

  FloatPoint3D point_in_view_space =
      inv_projection_.MapPoint(point_in_projection_space);

  const FloatPoint3D kOrigin(0.0, 0.0, 0.0);
  const FloatPoint3D kUp(0.0, 1.0, 0.0);

  // Generate a "Look At" matrix
  FloatPoint3D z_axis = kOrigin - point_in_view_space;
  z_axis.Normalize();

  FloatPoint3D x_axis = kUp.Cross(z_axis);
  x_axis.Normalize();

  FloatPoint3D y_axis = z_axis.Cross(x_axis);
  y_axis.Normalize();

  // TODO(bajones): There's probably a more efficent way to do this?
  TransformationMatrix inv_pointer(x_axis.X(), y_axis.X(), z_axis.X(), 0.0,
                                   x_axis.Y(), y_axis.Y(), z_axis.Y(), 0.0,
                                   x_axis.Z(), y_axis.Z(), z_axis.Z(), 0.0, 0.0,
                                   0.0, 0.0, 1.0);
  inv_pointer.Translate3d(-point_in_view_space.X(), -point_in_view_space.Y(),
                          -point_in_view_space.Z());

  // LookAt matrices are view matrices (inverted), so invert before returning.
  return inv_pointer.Inverse();
}

void XRViewData::SetHeadFromEyeTransform(
    const TransformationMatrix& head_from_eye) {
  head_from_eye_ = head_from_eye;
}

// ref_space_from_eye_ = ref_space_from_head * head_from_eye_
void XRViewData::UpdatePoseMatrix(
    const TransformationMatrix& ref_space_from_head) {
  ref_space_from_eye_ = ref_space_from_head;
  ref_space_from_eye_.Multiply(head_from_eye_);
}

XRRigidTransform* XRView::transform() const {
  return ref_space_from_eye_;
}

void XRView::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(projection_matrix_);
  visitor->Trace(ref_space_from_eye_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
