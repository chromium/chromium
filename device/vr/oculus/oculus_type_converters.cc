// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/oculus/oculus_type_converters.h"

#include <math.h>
#include <iterator>
#include <vector>

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"

namespace mojo {

device::mojom::VRPosePtr
TypeConverter<device::mojom::VRPosePtr, ovrPosef>::Convert(
    const ovrPosef& hmd_pose) {
  device::mojom::VRPosePtr pose = device::mojom::VRPose::New();
  pose->orientation =
      gfx::Quaternion(hmd_pose.Orientation.x, hmd_pose.Orientation.y,
                      hmd_pose.Orientation.z, hmd_pose.Orientation.w);
  pose->position = gfx::Point3F(hmd_pose.Position.x, hmd_pose.Position.y,
                                hmd_pose.Position.z);

  // TODO: If we want linear/angular velocity, we need to convert a
  // ovrPoseStatef.
  return pose;
}

}  // namespace mojo
