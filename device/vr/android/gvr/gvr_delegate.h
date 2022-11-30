// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_H_
#define DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_H_

#include <stdint.h>

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_export.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Transform;
}  // namespace gfx

namespace gvr {
class GvrApi;
}  // namespace gvr

namespace device {

class DEVICE_VR_EXPORT GvrDelegate {
 public:
  static mojom::VRPosePtr VRPosePtrFromGvrPose(const gfx::Transform& head_mat);
  static void GetGvrPoseWithNeckModel(gvr::GvrApi* gvr_api,
                                      gfx::Transform* out,
                                      int64_t prediction_time);
  static void GetGvrPoseWithNeckModel(gvr::GvrApi* gvr_api,
                                      gfx::Transform* out);
  static mojom::VRPosePtr GetVRPosePtrWithNeckModel(
      gvr::GvrApi* gvr_api,
      gfx::Transform* head_mat_out,
      int64_t prediction_time);
  static mojom::VRPosePtr GetVRPosePtrWithNeckModel(
      gvr::GvrApi* gvr_api,
      gfx::Transform* head_mat_out);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_GVR_DELEGATE_H_
