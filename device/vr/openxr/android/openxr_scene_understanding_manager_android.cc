// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/openxr/android/openxr_scene_understanding_manager_android.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace {
constexpr uint32_t kMaxHitTestResults = 2;
}  // namespace

namespace device {

OpenXRSceneUnderstandingManagerAndroid::
    ~OpenXRSceneUnderstandingManagerAndroid() = default;

OpenXRSceneUnderstandingManagerAndroid::OpenXRSceneUnderstandingManagerAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {}

bool OpenXRSceneUnderstandingManagerAndroid::OnNewHitTestSubscription() {
  if (plane_tracker_ == XR_NULL_HANDLE) {
    XrTrackableTrackerCreateInfoANDROID create_info{
        XR_TYPE_TRACKABLE_TRACKER_CREATE_INFO_ANDROID};
    create_info.trackableType = XR_TRACKABLE_TYPE_PLANE_ANDROID;

    XrResult result =
        extension_helper_->ExtensionMethods().xrCreateTrackableTrackerANDROID(
            session_, &create_info, &plane_tracker_);
    RETURN_VAL_IF_XR_FAILED(result, false);
  }

  return true;
}

void OpenXRSceneUnderstandingManagerAndroid::
    OnAllHitTestSubscriptionsRemoved() {
  if (plane_tracker_ != XR_NULL_HANDLE) {
    // We don't really need to handle if we fail to destroy the plane tracker,
    // but log it for future debuggability.
    XrResult result =
        extension_helper_->ExtensionMethods().xrDestroyTrackableTrackerANDROID(
            plane_tracker_);
    plane_tracker_ = XR_NULL_HANDLE;
    if (XR_FAILED(result)) {
      LOG(ERROR) << __func__ << " Could not destroy a plane tracker";
    }
  }
}

void OpenXRSceneUnderstandingManagerAndroid::OnFrameUpdate(
    XrTime predicted_display_time) {
  predicted_display_time_ = predicted_display_time;
}

std::vector<mojom::XRHitResultPtr>
OpenXRSceneUnderstandingManagerAndroid::RequestHitTest(
    const gfx::Point3F& ray_origin,
    const gfx::Vector3dF& ray_direction) {
  if (predicted_display_time_ == 0 || plane_tracker_ == XR_NULL_HANDLE) {
    DLOG(ERROR)
        << __func__
        << " Missing either predicted display time or plane tracker. Returning";
    return {};
  }

  XrRaycastInfoANDROID raycast_info = {XR_TYPE_RAYCAST_INFO_ANDROID};
  raycast_info.maxResults = kMaxHitTestResults;
  raycast_info.trackerCount = 1;
  raycast_info.trackers = &plane_tracker_;
  raycast_info.origin =
      XrVector3f{ray_origin.x(), ray_origin.y(), ray_origin.z()};
  raycast_info.trajectory =
      XrVector3f{ray_direction.x(), ray_direction.y(), ray_direction.z()};
  raycast_info.space = mojo_space_;
  raycast_info.time = predicted_display_time_;

  XrRaycastHitResultANDROID xr_results_array[kMaxHitTestResults];
  XrRaycastHitResultsANDROID xr_hit_results = {
      XR_TYPE_RAYCAST_HIT_RESULTS_ANDROID};
  xr_hit_results.resultsCapacityInput = kMaxHitTestResults;
  xr_hit_results.results = xr_results_array;

  RETURN_VAL_IF_XR_FAILED(
      extension_helper_->ExtensionMethods().xrRaycastANDROID(
          session_, &raycast_info, &xr_hit_results),
      {});

  // We receive the hit test results back in increasing distance from the item
  // that they hit, with the Y-direction matching the normal of the plane, and
  // in the space that we specified (which is mojo space), this all matches what
  // WebXR expects, so we simply have to convert the XrPosef to a device pose.
  std::vector<mojom::XRHitResultPtr> hit_results;
  hit_results.reserve(xr_hit_results.resultsCountOutput);
  for (uint32_t i = 0; i < xr_hit_results.resultsCountOutput; i++) {
    mojom::XRHitResultPtr result = mojom::XRHitResult::New();
    result->mojo_from_result = XrPoseToDevicePose(xr_results_array[i].pose);
    hit_results.push_back(std::move(result));
  }

  DVLOG(2) << __func__ << ": hit_results->size()=" << hit_results.size();
  return hit_results;
}

OpenXrSceneUnderstandingManagerAndroidFactory::
    OpenXrSceneUnderstandingManagerAndroidFactory() = default;
OpenXrSceneUnderstandingManagerAndroidFactory::
    ~OpenXrSceneUnderstandingManagerAndroidFactory() = default;

const base::flat_set<std::string_view>&
OpenXrSceneUnderstandingManagerAndroidFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions({
      XR_ANDROID_TRACKABLES_EXTENSION_NAME,
      XR_ANDROID_RAYCAST_EXTENSION_NAME,
  });

  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrSceneUnderstandingManagerAndroidFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::HIT_TEST};
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrSceneUnderstandingManagerAndroidFactory::CreateSceneUnderstandingManager(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXRSceneUnderstandingManagerAndroid>(
        extension_helper, session, mojo_space);
  }

  return nullptr;
}
}  // namespace device
