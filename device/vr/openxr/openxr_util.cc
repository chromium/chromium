// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_util.h"

#include <string>

#include "base/check_op.h"
#include "ui/gfx/geometry/angle_conversions.h"
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

XrPosef GfxTransformToXrPose(const gfx::Transform& transform) {
  absl::optional<gfx::DecomposedTransform> decomposed_transform =
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

bool IsPoseValid(XrSpaceLocationFlags locationFlags) {
  XrSpaceLocationFlags PoseValidFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  return (locationFlags & PoseValidFlags) == PoseValidFlags;
}

}  // namespace device
