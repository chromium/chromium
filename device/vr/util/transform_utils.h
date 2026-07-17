// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_UTIL_TRANSFORM_UTILS_H_
#define DEVICE_VR_UTIL_TRANSFORM_UTILS_H_

#include "base/component_export.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace gfx {
class Transform;
}  // namespace gfx

namespace device {
namespace vr_utils {

gfx::Transform COMPONENT_EXPORT(DEVICE_VR_UTIL)
    VrPoseToTransform(const device::mojom::VRPose* pose);

device::mojom::VRPosePtr COMPONENT_EXPORT(DEVICE_VR_UTIL)
    GfxTransformToVrPose(const gfx::Transform& transform,
                         bool emulated_position);

}  // namespace vr_utils
}  // namespace device

#endif  // DEVICE_VR_UTIL_TRANSFORM_UTILS_H_
