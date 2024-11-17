// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/openxr/openxr_util.h"

#include <string>

#include "base/check_op.h"
#include "base/numerics/angle_conversions.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
namespace device {

XrPosef PoseIdentity() {
  XrPosef pose{};
  pose.orientation.w = 1;
  return pose;
}

gfx::Transform XrPoseToGfxTransform(const XrPosef& pose) {
  gfx::DecomposedTransform decomp;
  decomp.quaternion = gfx::Quaternion(pose.orientation.x, pose.orientation.y,
                                      pose.orientation.z, pose.orientation.w);
  decomp.translate[0] = pose.position.x;
  decomp.translate[1] = pose.position.y;
  decomp.translate[2] = pose.position.z;

  return gfx::Transform::Compose(decomp);
}

device::Pose XrPoseToDevicePose(const XrPosef& pose) {
  gfx::Quaternion orientation{pose.orientation.x, pose.orientation.y,
                              pose.orientation.z, pose.orientation.w};
  gfx::Point3F position{pose.position.x, pose.position.y, pose.position.z};
  return device::Pose{position, orientation};
}

XrPosef GfxTransformToXrPose(const gfx::Transform& transform) {
  std::optional<gfx::DecomposedTransform> decomposed_transform =
      transform.Decompose();
  // This pose should always be a simple translation and rotation so this should
  // always be true
  DCHECK(decomposed_transform);
  return {{static_cast<float>(decomposed_transform->quaternion.x()),
           static_cast<float>(decomposed_transform->quaternion.y()),
           static_cast<float>(decomposed_transform->quaternion.z()),
           static_cast<float>(decomposed_transform->quaternion.w())},
          {static_cast<float>(decomposed_transform->translate[0]),
           static_cast<float>(decomposed_transform->translate[1]),
           static_cast<float>(decomposed_transform->translate[2])}};
}

mojom::VRFieldOfViewPtr XrFovToMojomFov(const XrFovf& xr_fov) {
  auto field_of_view = mojom::VRFieldOfView::New();
  field_of_view->up_degrees = base::RadToDeg(xr_fov.angleUp);
  field_of_view->down_degrees = base::RadToDeg(-xr_fov.angleDown);
  field_of_view->left_degrees = base::RadToDeg(-xr_fov.angleLeft);
  field_of_view->right_degrees = base::RadToDeg(xr_fov.angleRight);

  return field_of_view;
}

bool IsPoseValid(XrSpaceLocationFlags locationFlags) {
  XrSpaceLocationFlags PoseValidFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  return (locationFlags & PoseValidFlags) == PoseValidFlags;
}

bool IsArOnlyFeature(device::mojom::XRSessionFeature feature) {
  switch (feature) {
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
    case device::mojom::XRSessionFeature::LAYERS:
    case device::mojom::XRSessionFeature::HAND_INPUT:
    case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
    case device::mojom::XRSessionFeature::WEBGPU:
      return false;
    case device::mojom::XRSessionFeature::DOM_OVERLAY:
    case device::mojom::XRSessionFeature::HIT_TEST:
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
    case device::mojom::XRSessionFeature::ANCHORS:
    case device::mojom::XRSessionFeature::CAMERA_ACCESS:
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
    case device::mojom::XRSessionFeature::DEPTH:
    case device::mojom::XRSessionFeature::IMAGE_TRACKING:
    case device::mojom::XRSessionFeature::FRONT_FACING:
      return true;
  }
}

bool IsFeatureSupportedForMode(device::mojom::XRSessionFeature feature,
                               device::mojom::XRSessionMode mode) {
  // OpenXR doesn't support inline.
  CHECK_NE(mode, device::mojom::XRSessionMode::kInline);
  // If the feature is AR-only, then it's only supported if the mode is AR.
  if (IsArOnlyFeature(feature)) {
    return mode == device::mojom::XRSessionMode::kImmersiveAr;
  }

  // If the feature isn't AR-only, then it's supported.
  return true;
}

}  // namespace device
