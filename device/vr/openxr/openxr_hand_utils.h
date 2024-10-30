// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_HAND_UTILS_H_
#define DEVICE_VR_OPENXR_OPENXR_HAND_UTILS_H_

#include "device/vr/public/mojom/xr_hand_tracking_data.mojom-forward.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom-shared.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

inline constexpr unsigned kNumWebXRJoints =
    static_cast<unsigned>(mojom::XRHandJoint::kMaxValue) + 1u;

constexpr mojom::XRHandJoint OpenXRHandJointToMojomJoint(
    XrHandJointEXT openxr_joint) {
  CHECK_NE(openxr_joint, XR_HAND_JOINT_PALM_EXT);
  // The OpenXR joints have palm at 0, but from that point are the same as the
  // mojom joints. Hence they are offset by 1.
  return static_cast<mojom::XRHandJoint>(openxr_joint - 1);
}

bool DEVICE_VR_EXPORT
AnonymizeHand(base::span<mojom::XRHandJointDataPtr> hand_data);

}  // namespace device
#endif  // DEVICE_VR_OPENXR_OPENXR_HAND_UTILS_H_
