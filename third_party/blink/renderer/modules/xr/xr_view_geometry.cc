// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "third_party/blink/renderer/modules/xr/xr_view_geometry.h"

#include <cmath>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"
#include "ui/gfx/geometry/transform.h"

namespace {
const double kDegToRad = M_PI / 180.0;

constexpr float kDefaultNearDepth = 0.0001;
constexpr float kDefaultFarDepth = 10000;
}

namespace blink {

XRViewGeometry::XRViewGeometry(XRGraphicsBinding::Api graphics_api)
    : graphics_api_(graphics_api) {}

XRViewGeometry::XRViewGeometry(
    const device::mojom::blink::XRViewGeometryPtr& view_geometry,
    XRGraphicsBinding::Api graphics_api)
    : graphics_api_(graphics_api) {
  CHECK(view_geometry);
  UpdateViewGeometry(view_geometry, kDefaultNearDepth, kDefaultFarDepth);
}

void XRViewGeometry::UpdateViewGeometry(
    const device::mojom::blink::XRViewGeometryPtr& view_geometry,
    double depth_near,
    double depth_far) {
  CHECK(view_geometry);

  const auto& fov = view_geometry->field_of_view;
  UpdateProjectionMatrixFromFoV(
      fov->up_degrees * kDegToRad, fov->down_degrees * kDegToRad,
      fov->left_degrees * kDegToRad, fov->right_degrees * kDegToRad, depth_near,
      depth_far);

  mojo_from_view_ = view_geometry->mojo_from_view;
}

void XRViewGeometry::UpdateProjectionMatrixFromFoV(float up_rad,
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

  // Compute the appropriate matrix for the graphics API being used.
  // WebGPU uses a clip space with a depth range of [0, 1], which requires a
  // different projection matrix than WebGL, which uses a clip space with a
  // depth range of [-1, 1].
  if (graphics_api_ == XRGraphicsBinding::Api::kWebGPU) {
    projection_matrix_ = gfx::Transform::ColMajor(
        x_scale, 0.0f, 0.0f, 0.0f, 0.0f, y_scale, 0.0f, 0.0f,
        -((left_tan - right_tan) * x_scale * 0.5),
        ((up_tan - down_tan) * y_scale * 0.5), far_depth * inv_nf, -1.0f, 0.0f,
        0.0f, far_depth * near_depth * inv_nf, 0.0f);
  } else {
    projection_matrix_ = gfx::Transform::ColMajor(
        x_scale, 0.0f, 0.0f, 0.0f, 0.0f, y_scale, 0.0f, 0.0f,
        -((left_tan - right_tan) * x_scale * 0.5),
        ((up_tan - down_tan) * y_scale * 0.5),
        (near_depth + far_depth) * inv_nf, -1.0f, 0.0f, 0.0f,
        (2.0f * far_depth * near_depth) * inv_nf, 0.0f);
  }
}

void XRViewGeometry::UpdateProjectionMatrixFromAspect(float fovy,
                                                      float aspect,
                                                      float near_depth,
                                                      float far_depth) {
  float f = 1.0f / tanf(fovy / 2);
  float inv_nf = 1.0f / (near_depth - far_depth);

  if (graphics_api_ == XRGraphicsBinding::Api::kWebGPU) {
    projection_matrix_ = gfx::Transform::ColMajor(
        f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f,
        far_depth * inv_nf, -1.0f, 0.0f, 0.0f, far_depth * near_depth * inv_nf,
        0.0f);
  } else {
    projection_matrix_ = gfx::Transform::ColMajor(
        f / aspect, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f,
        (far_depth + near_depth) * inv_nf, -1.0f, 0.0f, 0.0f,
        (2.0f * far_depth * near_depth) * inv_nf, 0.0f);
  }

  inv_projection_dirty_ = true;
}

gfx::Transform XRViewGeometry::UnprojectPointer(double x,
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

void XRViewGeometry::SetMojoFromView(const gfx::Transform& mojo_from_view) {
  mojo_from_view_ = mojo_from_view;
}
}  // namespace blink
