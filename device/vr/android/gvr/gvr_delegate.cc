// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_delegate.h"

#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_utils.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {
// TODO(mthiesse): If gvr::PlatformInfo().GetPosePredictionTime() is ever
// exposed, use that instead (it defaults to 50ms on most platforms).
static constexpr int64_t kPredictionTimeWithoutVsyncNanos = 50000000;

}  // namespace

/* static */
mojom::VRPosePtr GvrDelegate::VRPosePtrFromGvrPose(
    const gfx::Transform& head_mat) {
  mojom::VRPosePtr pose = mojom::VRPose::New();

  pose->orientation = gfx::Quaternion();

  gfx::Transform inv_transform(head_mat);

  gfx::Transform transform;
  if (inv_transform.GetInverse(&transform)) {
    gfx::DecomposedTransform decomposed_transform;
    gfx::DecomposeTransform(&decomposed_transform, transform);

    pose->orientation = decomposed_transform.quaternion;

    pose->position = gfx::Point3F(decomposed_transform.translate[0],
                                  decomposed_transform.translate[1],
                                  decomposed_transform.translate[2]);
  }

  return pose;
}

/* static */
void GvrDelegate::GetGvrPoseWithNeckModel(gvr::GvrApi* gvr_api,
                                          gfx::Transform* out,
                                          int64_t prediction_time) {
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += prediction_time;

  gvr::Mat4f head_mat = gvr_api->ApplyNeckModel(
      gvr_api->GetHeadSpaceFromStartSpaceRotation(target_time), 1.0f);

  gvr_utils::GvrMatToTransform(head_mat, out);
}

/* static */
void GvrDelegate::GetGvrPoseWithNeckModel(gvr::GvrApi* gvr_api,
                                          gfx::Transform* out) {
  GetGvrPoseWithNeckModel(gvr_api, out, kPredictionTimeWithoutVsyncNanos);
}

/* static */
mojom::VRPosePtr GvrDelegate::GetVRPosePtrWithNeckModel(
    gvr::GvrApi* gvr_api,
    gfx::Transform* head_mat_out,
    int64_t prediction_time) {
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += prediction_time;

  gvr::Mat4f gvr_head_mat = gvr_api->ApplyNeckModel(
      gvr_api->GetHeadSpaceFromStartSpaceRotation(target_time), 1.0f);

  gfx::Transform* head_mat_ptr = head_mat_out;
  gfx::Transform head_mat;
  if (!head_mat_ptr)
    head_mat_ptr = &head_mat;
  gvr_utils::GvrMatToTransform(gvr_head_mat, head_mat_ptr);

  mojom::VRPosePtr pose = GvrDelegate::VRPosePtrFromGvrPose(*head_mat_ptr);

  // The position is emulated unless the current tracking status is 6DoF and is
  // not still initializing or invalid.
  pose->emulated_position = true;
  gvr::Properties props = gvr_api->GetCurrentProperties();
  gvr::Value head_tracking_status;
  if (props.Get(GVR_PROPERTY_TRACKING_STATUS, &head_tracking_status)) {
    bool has_6dof =
        !!(head_tracking_status.fl & GVR_TRACKING_STATUS_FLAG_HAS_6DOF);
    bool initialized =
        !(head_tracking_status.fl & GVR_TRACKING_STATUS_FLAG_INITIALIZING);
    bool valid = !(head_tracking_status.fl & GVR_TRACKING_STATUS_FLAG_INVALID);
    if (has_6dof && initialized && valid) {
      pose->emulated_position = false;
    }
  }

  return pose;
}

/* static */
mojom::VRPosePtr GvrDelegate::GetVRPosePtrWithNeckModel(
    gvr::GvrApi* gvr_api,
    gfx::Transform* head_mat_out) {
  return GetVRPosePtrWithNeckModel(gvr_api, head_mat_out,
                                   kPredictionTimeWithoutVsyncNanos);
}

}  // namespace device
