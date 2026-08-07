// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/android/openxr_hit_test_manager_android.h"

#include <array>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "device/vr/openxr/android/openxr_plane_manager_android.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace {
constexpr size_t kMaxHitTestResults = 2;
}  // namespace

namespace device {

OpenXrHitTestManagerAndroid::OpenXrHitTestManagerAndroid(
    OpenXrPlaneManagerAndroid* plane_manager,
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : plane_manager_(plane_manager),
      extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {}

OpenXrHitTestManagerAndroid::~OpenXrHitTestManagerAndroid() = default;

std::vector<mojom::XRHitResultPtr> OpenXrHitTestManagerAndroid::RequestHitTest(
    const gfx::Point3F& ray_origin,
    const gfx::Vector3dF& ray_direction) {
  if (!plane_manager_) {
    DVLOG(3) << __func__ << ": plane_manager_ not available";
    return {};
  }

  XrTrackableTrackerANDROID plane_tracker = plane_manager_->plane_tracker();

  if (predicted_display_time_ == 0 || plane_tracker == XR_NULL_HANDLE ||
      mojo_space_ == XR_NULL_HANDLE) {
    DVLOG(3) << __func__
             << ": plane_manager_ not yet ready for hit-testing. "
                "predicted_display_time="
             << predicted_display_time_ << ", plane_tracker=" << plane_tracker
             << ", mojo_space=" << mojo_space_;
    return {};
  }

  XrRaycastInfoANDROID raycast_info = {XR_TYPE_RAYCAST_INFO_ANDROID};
  raycast_info.maxResults = kMaxHitTestResults;
  raycast_info.trackerCount = 1;
  raycast_info.trackers = &plane_tracker;
  raycast_info.origin =
      XrVector3f{ray_origin.x(), ray_origin.y(), ray_origin.z()};
  raycast_info.trajectory =
      XrVector3f{ray_direction.x(), ray_direction.y(), ray_direction.z()};
  raycast_info.space = mojo_space_;
  raycast_info.time = predicted_display_time_;

  std::array<XrRaycastHitResultANDROID, kMaxHitTestResults> xr_results_array;
  XrRaycastHitResultsANDROID xr_hit_results = {
      XR_TYPE_RAYCAST_HIT_RESULTS_ANDROID};
  xr_hit_results.resultsCapacityInput = xr_results_array.size();
  xr_hit_results.results = xr_results_array.data();

  RETURN_VAL_IF_XR_FAILED(
      extension_helper_->ExtensionMethods().xrRaycastANDROID(
          session_, &raycast_info, &xr_hit_results),
      {});

  // The successful call guarantees that `resultsCountOutput` valid results were
  // populated by the runtime. Subspan the array to easily and safely iterate
  // over only the valid returned results.
  CHECK_LE(xr_hit_results.resultsCountOutput, xr_results_array.size());
  auto xr_results =
      base::span(xr_results_array).first(xr_hit_results.resultsCountOutput);

  // We receive the hit test results back in increasing distance from the item
  // that they hit, with the Y-direction matching the normal of the plane, and
  // in the space that we specified (which is mojo space), this all matches what
  // WebXR expects, so we simply have to convert the XrPosef to a device pose.
  std::vector<mojom::XRHitResultPtr> hit_results;
  hit_results.reserve(xr_results.size());
  for (const auto& xr_result : xr_results) {
    mojom::XRHitResultPtr result = mojom::XRHitResult::New();
    result->mojo_from_result = XrPoseToDevicePose(xr_result.pose);
    hit_results.push_back(std::move(result));
  }

  DVLOG(2) << __func__ << ": hit_results->size()=" << hit_results.size();
  return hit_results;
}

void OpenXrHitTestManagerAndroid::OnStartProcessingHitTests(
    XrTime predicted_display_time) {
  predicted_display_time_ = predicted_display_time;
}

}  // namespace device
