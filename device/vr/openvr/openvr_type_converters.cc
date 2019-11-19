// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_type_converters.h"

#include <math.h>
#include <iterator>
#include <vector>

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/transform_util.h"

namespace mojo {

device::mojom::VRPosePtr
TypeConverter<device::mojom::VRPosePtr, vr::TrackedDevicePose_t>::Convert(
    const vr::TrackedDevicePose_t& hmd_pose) {
  device::mojom::VRPosePtr pose = device::mojom::VRPose::New();
  pose->orientation = gfx::Quaternion();
  pose->position = gfx::Point3F();

  if (hmd_pose.bPoseIsValid && hmd_pose.bDeviceIsConnected) {
    const float(&m)[3][4] = hmd_pose.mDeviceToAbsoluteTracking.m;

    gfx::Transform transform = gfx::Transform(
        m[0][0], m[0][1], m[0][2], 0.0f, m[1][0], m[1][1], m[1][2], 0.0f,
        m[2][0], m[2][1], m[2][2], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    gfx::DecomposedTransform decomposed;
    if (gfx::DecomposeTransform(&decomposed, transform)) {
      pose->orientation = decomposed.quaternion;
    }

    pose->position->SetPoint(m[0][3], m[1][3], m[2][3]);
  }

  return pose;
}

}  // namespace mojo
