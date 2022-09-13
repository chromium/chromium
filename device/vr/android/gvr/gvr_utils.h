// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_GVR_UTILS_H_
#define DEVICE_VR_ANDROID_GVR_GVR_UTILS_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_export.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace gfx {
class Transform;
}  // namespace gfx

namespace device {
namespace gvr_utils {

std::vector<device::mojom::XRViewPtr> DEVICE_VR_EXPORT
CreateViews(gvr::GvrApi* gvr_api, const device::mojom::VRPose* head_pose);

void GvrMatToTransform(const gvr::Mat4f& in, gfx::Transform* out);

}  // namespace gvr_utils
}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_GVR_UTILS_H_
