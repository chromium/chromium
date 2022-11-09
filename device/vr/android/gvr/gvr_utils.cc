// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_utils.h"

#include "device/vr/util/transform_utils.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace {

gfx::Size GetMaximumWebVrSize(gvr::GvrApi* gvr_api) {
  // Get the default, unscaled size for the WebVR transfer surface
  // based on the optimal 1:1 render resolution. A scalar will be applied to
  // this value in the renderer to reduce the render load. This size will also
  // be reported to the client via XRViews (initially on XRSessionDeviceConfig,
  // and then on XRFrameData) as the client-recommended
  // render_width/render_height and for the GVR framebuffer. If the client
  // chooses a different size or resizes it while presenting, we'll resize the
  // transfer surface and GVR framebuffer to match.
  gvr::Sizei render_target_size =
      gvr_api->GetMaximumEffectiveRenderTargetSize();

  gfx::Size webvr_size(render_target_size.width, render_target_size.height);

  // Ensure that the width is an even number so that the eyes each
  // get the same size, the recommended render_width is per eye
  // and the client will use the sum of the left and right width.
  //
  // TODO(https://crbug.com/699350): should we round the recommended
  // size to a multiple of 2^N pixels to be friendlier to the GPU? The
  // exact size doesn't matter, and it might be more efficient.
  webvr_size.set_width(webvr_size.width() & ~1);
  return webvr_size;
}

device::mojom::XRViewPtr CreateView(
    gvr::GvrApi* gvr_api,
    gvr::Eye eye,
    const gvr::BufferViewportList& buffers,
    const gfx::Size& maximum_size,
    const device::mojom::VRPose* mojo_from_head_pose) {
  device::mojom::XRViewPtr view = device::mojom::XRView::New();

  if (eye == GVR_LEFT_EYE) {
    view->eye = device::mojom::XREye::kLeft;
    view->viewport =
        gfx::Rect(0, 0, maximum_size.width() / 2, maximum_size.height());
  } else if (eye == GVR_RIGHT_EYE) {
    view->eye = device::mojom::XREye::kRight;
    view->viewport = gfx::Rect(maximum_size.width() / 2, 0,
                               maximum_size.width() / 2, maximum_size.height());
  } else {
    NOTREACHED();
  }

  view->field_of_view = device::mojom::VRFieldOfView::New();

  gvr::BufferViewport eye_viewport = gvr_api->CreateBufferViewport();
  buffers.GetBufferViewport(eye, &eye_viewport);
  gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
  view->field_of_view->up_degrees = eye_fov.top;
  view->field_of_view->down_degrees = eye_fov.bottom;
  view->field_of_view->left_degrees = eye_fov.left;
  view->field_of_view->right_degrees = eye_fov.right;

  if (mojo_from_head_pose) {
    gfx::Transform mojo_from_head =
        device::vr_utils::VrPoseToTransform(mojo_from_head_pose);
    gvr::Mat4f eye_mat = gvr_api->GetEyeFromHeadMatrix(eye);
    gfx::Transform eye_from_head;
    device::gvr_utils::GvrMatToTransform(eye_mat, &eye_from_head);
    gfx::Transform head_from_eye = eye_from_head.GetCheckedInverse();

    view->mojo_from_view = mojo_from_head * head_from_eye;
  }

  return view;
}

}  // namespace

namespace device {
namespace gvr_utils {

void GvrMatToTransform(const gvr::Mat4f& in, gfx::Transform* out) {
  *out = gfx::Transform::RowMajor(
      in.m[0][0], in.m[0][1], in.m[0][2], in.m[0][3], in.m[1][0], in.m[1][1],
      in.m[1][2], in.m[1][3], in.m[2][0], in.m[2][1], in.m[2][2], in.m[2][3],
      in.m[3][0], in.m[3][1], in.m[3][2], in.m[3][3]);
}

std::vector<device::mojom::XRViewPtr> CreateViews(
    gvr::GvrApi* gvr_api,
    const device::mojom::VRPose* mojo_from_head_pose) {
  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();
  gfx::Size maximum_size = GetMaximumWebVrSize(gvr_api);

  std::vector<device::mojom::XRViewPtr> views(2);
  views[0] = CreateView(gvr_api, GVR_LEFT_EYE, gvr_buffer_viewports,
                        maximum_size, mojo_from_head_pose);
  views[1] = CreateView(gvr_api, GVR_RIGHT_EYE, gvr_buffer_viewports,
                        maximum_size, mojo_from_head_pose);

  return views;
}

}  // namespace gvr_utils
}  // namespace device
