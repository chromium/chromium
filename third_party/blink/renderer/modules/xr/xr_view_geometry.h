// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEW_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEW_GEOMETRY_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {
class XRViewGeometry {
 public:
  explicit XRViewGeometry(XRGraphicsBinding::Api graphics_api);
  XRViewGeometry(const device::mojom::blink::XRViewGeometryPtr& view_geometry,
                 XRGraphicsBinding::Api graphics_api);

  void UpdateViewGeometry(
      const device::mojom::blink::XRViewGeometryPtr& view_geometry,
      double depth_near,
      double depth_far);

  void UpdateProjectionMatrixFromFoV(float up_rad,
                                     float down_rad,
                                     float left_rad,
                                     float right_rad,
                                     float near_depth,
                                     float far_depth);
  void UpdateProjectionMatrixFromAspect(float fovy,
                                        float aspect,
                                        float near_depth,
                                        float far_depth);

  gfx::Transform UnprojectPointer(double x,
                                  double y,
                                  double canvas_width,
                                  double canvas_height);

  void SetMojoFromView(const gfx::Transform& mojo_from_view);
  const gfx::Transform& MojoFromView() const { return mojo_from_view_; }
  const gfx::Transform& ProjectionMatrix() const { return projection_matrix_; }

 private:
  gfx::Transform mojo_from_view_;
  gfx::Transform projection_matrix_;
  gfx::Transform inv_projection_;
  bool inv_projection_dirty_ = true;
  const XRGraphicsBinding::Api graphics_api_;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEW_GEOMETRY_H_
