// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/transform_utils.h"

#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/transform.h"

namespace device {
namespace vr_utils {

gfx::Transform VrPoseToTransform(const device::mojom::VRPose* pose) {
  gfx::DecomposedTransform decomp;
  if (pose->orientation) {
    decomp.quaternion =
        gfx::Quaternion(pose->orientation->x(), pose->orientation->y(),
                        pose->orientation->z(), pose->orientation->w());
  }
  if (pose->position) {
    decomp.translate[0] = pose->position->x();
    decomp.translate[1] = pose->position->y();
    decomp.translate[2] = pose->position->z();
  }

  return gfx::Transform::Compose(decomp);
}

device::mojom::VRPosePtr GfxTransformToVrPose(const gfx::Transform& transform,
                                              bool emulated_position) {
  std::optional<gfx::DecomposedTransform> decomp = transform.Decompose();
  if (!decomp) {
    return nullptr;
  }

  device::mojom::VRPosePtr pose = device::mojom::VRPose::New();
  pose->position = gfx::Point3F(decomp->translate[0], decomp->translate[1],
                                decomp->translate[2]);
  pose->orientation = decomp->quaternion;
  pose->emulated_position = emulated_position;
  return pose;
}

}  // namespace vr_utils
}  // namespace device
