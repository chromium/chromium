// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "third_party/blink/renderer/modules/xr/xr_view.h"

#include <cmath>

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/modules/xr/xr_camera.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "ui/gfx/geometry/point3_f.h"

namespace blink {

namespace {

// Arbitrary minimum size multiplier for dynamic viewport scaling,
// where 1.0 is full framebuffer size (which may in turn be adjusted
// by framebufferScaleFactor). This should be less than or equal to
// kMinScale in xr_session_viewport_scaler.cc to allow use of the full
// dynamic viewport scaling range.
constexpr double kMinViewportScale = 0.125;

const double kDegToRad = M_PI / 180.0;

}  // namespace

XRView::XRView(XRFrame* frame,
               XRViewData* view_data,
               const gfx::Transform& ref_space_from_mojo)
    : eye_(view_data->Eye()), frame_(frame), view_data_(view_data) {
  switch (eye_) {
    case device::mojom::blink::XREye::kLeft:
      eye_string_ = "left";
      break;
    case device::mojom::blink::XREye::kRight:
      eye_string_ = "right";
      break;
    default:
      eye_string_ = "none";
  }
  ref_space_from_view_ = MakeGarbageCollected<XRRigidTransform>(
      ref_space_from_mojo * view_data->MojoFromView());
  projection_matrix_ =
      transformationMatrixToDOMFloat32Array(view_data->ProjectionMatrix());
}

XRViewport* XRView::Viewport(double framebuffer_scale) {
  if (!viewport_) {
    const gfx::Rect& viewport = view_data_->Viewport();
    double scale = framebuffer_scale * view_data_->CurrentViewportScale();

    viewport_ = MakeGarbageCollected<XRViewport>(
        viewport.x() * scale, viewport.y() * scale, viewport.width() * scale,
        viewport.height() * scale);
  }

  return viewport_;
}

XRFrame* XRView::frame() const {
  return frame_;
}

XRSession* XRView::session() const {
  return frame_->session();
}

DOMFloat32Array* XRView::projectionMatrix() const {
  if (!projection_matrix_ || !projection_matrix_->Data()) {
    // A page may take the projection matrix value and detach it so
    // projection_matrix_ is a detached array buffer.  This breaks the
    // inspector, so return null instead.
    return nullptr;
  }

  return projection_matrix_;
}

XRViewData::XRViewData(const device::mojom::blink::XRViewPtr& view,
                       double depth_near,
                       double depth_far)
    : eye_(view->eye), viewport_(view->viewport) {
  UpdateView(view, depth_near, depth_far);
}

void XRViewData::UpdateView(const device::mojom::blink::XRViewPtr& view,
                            double depth_near,
                            double depth_far) {
  DCHECK_EQ(eye_, view->eye);

  const device::mojom::blink::VRFieldOfViewPtr& fov = view->field_of_view;
  UpdateProjectionMatrixFromFoV(
      fov->up_degrees * kDegToRad, fov->down_degrees * kDegToRad,
      fov->left_degrees * kDegToRad, fov->right_degrees * kDegToRad, depth_near,
      depth_far);

  mojo_from_view_ = view->mojo_from_view;

  viewport_ = view->viewport;
  is_first_person_observer_ = view->is_first_person_observer;
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

  projection_matrix_ = gfx::Transform::ColMajor(
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

  projection_matrix_ = gfx::Transform::ColMajor(
      f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f,
      (far_depth + near_depth) * inv_nf, -1.0f, 0.0f, 0.0f,
      (2.0f * far_depth * near_depth) * inv_nf, 0.0f);

  inv_projection_dirty_ = true;
}

gfx::Transform XRViewData::UnprojectPointer(double x,
                                            double y,
                                            double canvas_width,
                                            double canvas_height) {
  // Recompute the inverse projection matrix if needed.
  if (inv_projection_dirty_) {
    inv_projection_ = projection_matrix_.InverseOrIdentity();
    inv_projection_dirty_ = false;
  }

  // Transform the x/y coordinate into WebGL normalized device coordinates.
  // Z coordinate of -1 means the point will be projected onto the projection
  // matrix near plane.
  gfx::Point3F point_in_projection_space(
      x / canvas_width * 2.0 - 1.0,
      (canvas_height - y) / canvas_height * 2.0 - 1.0, -1.0);

  gfx::Point3F point_in_view_space =
      inv_projection_.MapPoint(point_in_projection_space);

  const gfx::Vector3dF kUp(0.0, 1.0, 0.0);

  // Generate a "Look At" matrix
  gfx::Vector3dF z_axis = -point_in_view_space.OffsetFromOrigin();
  z_axis.GetNormalized(&z_axis);

  gfx::Vector3dF x_axis = gfx::CrossProduct(kUp, z_axis);
  x_axis.GetNormalized(&x_axis);

  gfx::Vector3dF y_axis = gfx::CrossProduct(z_axis, x_axis);
  y_axis.GetNormalized(&y_axis);

  // TODO(bajones): There's probably a more efficient way to do this?
  auto inv_pointer = gfx::Transform::ColMajor(
      x_axis.x(), y_axis.x(), z_axis.x(), 0.0, x_axis.y(), y_axis.y(),
      z_axis.y(), 0.0, x_axis.z(), y_axis.z(), z_axis.z(), 0.0, 0.0, 0.0, 0.0,
      1.0);
  inv_pointer.Translate3d(-point_in_view_space.x(), -point_in_view_space.y(),
                          -point_in_view_space.z());

  // LookAt matrices are view matrices (inverted), so invert before returning.
  return inv_pointer.InverseOrIdentity();
}

XRRigidTransform* XRView::refSpaceFromView() const {
  return ref_space_from_view_;
}

absl::optional<double> XRView::recommendedViewportScale() const {
  return view_data_->recommendedViewportScale();
}

void XRView::requestViewportScale(absl::optional<double> scale) {
  view_data_->requestViewportScale(scale);
}

XRCamera* XRView::camera() const {
  const bool camera_access_enabled = frame_->session()->IsFeatureEnabled(
      device::mojom::XRSessionFeature::CAMERA_ACCESS);
  const bool is_immersive_ar_session =
      frame_->session()->mode() ==
      device::mojom::blink::XRSessionMode::kImmersiveAr;

  DVLOG(3) << __func__ << ": camera_access_enabled=" << camera_access_enabled
           << ", is_immersive_ar_session=" << is_immersive_ar_session;

  if (camera_access_enabled && is_immersive_ar_session) {
    // The feature is enabled and we're in immersive-ar session, so let's return
    // a camera object if the camera image was received in the current frame.
    // Note: currently our only implementation of AR sessions is provided by
    // ARCore device, which should *not* return a frame data with camera image
    // that is not set in case the raw camera access is enabled, so we could
    // DCHECK that the camera image size has value. Since there may be other AR
    // devices that implement raw camera access via a different mechanism that's
    // not neccessarily frame-aligned, a DCHECK here would affect them.
    if (frame_->session()->CameraImageSize().has_value()) {
      return MakeGarbageCollected<XRCamera>(frame_);
    }
  }

  return nullptr;
}

bool XRView::isFirstPersonObserver() const {
  return view_data_->IsFirstPersonObserver();
}

void XRView::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(projection_matrix_);
  visitor->Trace(ref_space_from_view_);
  visitor->Trace(view_data_);
  visitor->Trace(viewport_);
  ScriptWrappable::Trace(visitor);
}

absl::optional<double> XRViewData::recommendedViewportScale() const {
  return recommended_viewport_scale_;
}

void XRViewData::requestViewportScale(absl::optional<double> scale) {
  if (!scale)
    return;

  requested_viewport_scale_ = base::clamp(*scale, kMinViewportScale, 1.0);
}

}  // namespace blink
