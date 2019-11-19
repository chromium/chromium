// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_GVR_UTILS_H_
#define DEVICE_VR_ANDROID_GVR_GVR_UTILS_H_

#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace gfx {
class Transform;
}  // namespace gfx

namespace device {
namespace gvr_utils {

void GvrMatToTransform(const gvr::Mat4f& in, gfx::Transform* out);

}  // namespace gvr_utils
}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_GVR_UTILS_H_
