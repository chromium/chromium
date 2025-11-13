// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_hit_test_manager.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "device/vr/openxr/openxr_spatial_capability_configuration_base.h"
#include "device/vr/openxr/openxr_spatial_framework_manager.h"
#include "device/vr/openxr/openxr_spatial_plane_manager.h"
#include "device/vr/openxr/openxr_spatial_utils.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/scoped_openxr_object.h"
#include "device/vr/public/cpp/features.h"
#include "third_party/openxr/dev/xr_android.h"

namespace device {
namespace {
struct SpatialHitTestResult {
  XrSpatialEntityIdEXT id;
  device::Pose pose;
  float distance_squared;
};

bool SupportsPlaneBasedHitTest(
    XrInstance instance,
    XrSystemId system,
    PFN_xrEnumerateSpatialCapabilityComponentTypesEXT
        xrEnumerateSpatialCapabilityComponentTypesEXT,
    const std::vector<XrSpatialCapabilityEXT>& capabilities) {
  // To check for plane based hit test, we'll need to check for the
  // XR_SPATIAL_COMPONENT_TYPE_RAYCAST_RESULT_ANDROID component on
  // XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT, which is not guaranteed.
  if (!base::Contains(capabilities, XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT)) {
    return false;
  }

  std::vector<XrSpatialComponentTypeEXT> plane_tracking_components =
      GetSupportedComponentTypes(xrEnumerateSpatialCapabilityComponentTypesEXT,
                                 instance, system,
                                 XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT);
  return base::Contains(plane_tracking_components,
                        XR_SPATIAL_COMPONENT_TYPE_RAYCAST_RESULT_ANDROID);
}

bool SupportsDepthBasedHitTest(
    const std::vector<XrSpatialCapabilityEXT>& capabilities) {
  if (!base::FeatureList::IsEnabled(features::kSpatialEntitesDepthHitTest)) {
    return false;
  }

  // The only component that we need to support depth-based hit tests is
  // XR_SPATIAL_COMPONENT_TYPE_RAYCAST_RESULT_ANDROID, which is guaranteed to be
  // supported if the XR_SPATIAL_CAPABILITY_DEPTH_RAYCAST_ANDROID is supported,
  // so that's all we need to check for it.
  return base::Contains(capabilities,
                        XR_SPATIAL_CAPABILITY_DEPTH_RAYCAST_ANDROID);
}
}  // namespace

// static
bool OpenXrSpatialHitTestManager::IsSupported(
    XrInstance instance,
    XrSystemId system,
    PFN_xrEnumerateSpatialCapabilityComponentTypesEXT
        xrEnumerateSpatialCapabilityComponentTypesEXT,
    const std::vector<XrSpatialCapabilityEXT>& capabilities) {
  const bool supports_depth_hit_test = SupportsDepthBasedHitTest(capabilities);

  const bool supports_plane_based_hit_test = SupportsPlaneBasedHitTest(
      instance, system, xrEnumerateSpatialCapabilityComponentTypesEXT,
      capabilities);

  return supports_depth_hit_test || supports_plane_based_hit_test;
}

OpenXrSpatialHitTestManager::OpenXrSpatialHitTestManager(
    const OpenXrExtensionHelper& extension_helper,
    const OpenXrSpatialFrameworkManager& spatial_framework_manager,
    OpenXrSpatialPlaneManager* plane_manager,
    XrSpace mojo_space,
    XrInstance instance,
    XrSystemId system)
    : extension_helper_(extension_helper),
      spatial_framework_manager_(spatial_framework_manager),
      plane_manager_(plane_manager),
      mojo_space_(mojo_space),
      instance_(instance),
      system_(system) {}
OpenXrSpatialHitTestManager::~OpenXrSpatialHitTestManager() = default;

void OpenXrSpatialHitTestManager::PopulateCapabilityConfiguration(
    absl::flat_hash_map<XrSpatialCapabilityEXT,
                        absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
        capability_components) const {
  // We need to query the capabilities to determine which ones to enable.
  std::vector<XrSpatialCapabilityEXT> capabilities = GetCapabilities(
      extension_helper_->ExtensionMethods().xrEnumerateSpatialCapabilitiesEXT,
      instance_, system_);

  // If depth-based hit test is supported, we can enable it.
  if (SupportsDepthBasedHitTest(capabilities)) {
    DVLOG(1) << __func__ << " Enabling depth hit test";
    capability_components[XR_SPATIAL_CAPABILITY_DEPTH_RAYCAST_ANDROID].insert(
        XR_SPATIAL_COMPONENT_TYPE_RAYCAST_RESULT_ANDROID);
  }

  // If plane-based hit test is supported, we can enable it.
  if (SupportsPlaneBasedHitTest(
          instance_, system_,
          extension_helper_->ExtensionMethods()
              .xrEnumerateSpatialCapabilityComponentTypesEXT,
          capabilities)) {
    DVLOG(1) << __func__ << " Enabling plane hit test";
    capability_components[XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT].insert(
        XR_SPATIAL_COMPONENT_TYPE_RAYCAST_RESULT_ANDROID);
  }
}

XrSpatialSnapshotEXT OpenXrSpatialHitTestManager::GetSnapshot(
    const gfx::Point3F& origin,
    const gfx::Vector3dF& direction) {
  DVLOG(3) << __func__ << " origin=" << origin.ToString()
           << " direction=" << direction.ToString();

  // In order to get hit test data from a snapshot that snapshot must be created
  // with our ray passed in.
  XrSpatialContextEXT spatial_context =
      spatial_framework_manager_->GetSpatialContext();
  if (spatial_context == XR_NULL_HANDLE) {
    // Cannot query a snapshot without the context being ready.
    return XR_NULL_HANDLE;
  }

  XrSpatialBoundsRaycastANDROID raycast_info = {
      .type = XR_TYPE_SPATIAL_BOUNDS_RAYCAST_ANDROID,
      .space = mojo_space_,
      .time = predicted_display_time_,
      .origin = {origin.x(), origin.y(), origin.z()},
      .direction = {direction.x(), direction.y(), direction.z()},
      .maxDistance = 0.0f,  // Unbounded
  };

  // We only need to create a snapshot with the raycast results. It's the same
  // component type for both plane and depth based hit tests.
  std::array<XrSpatialComponentTypeEXT, 1> enabled_components = {
      XR_SPATIAL_COMPONENT_TYPE_RAYCAST_RESULT_ANDROID};
  XrSpatialDiscoverySnapshotCreateInfoEXT snapshot_create_info = {
      .type = XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT,
      .next = &raycast_info,
      .componentTypeCount = static_cast<uint32_t>(enabled_components.size()),
      .componentTypes = enabled_components.data(),
  };

  XrFutureEXT future;
  if (XR_FAILED(extension_helper_->ExtensionMethods()
                    .xrCreateSpatialDiscoverySnapshotAsyncEXT(
                        spatial_context, &snapshot_create_info, &future))) {
    DLOG(ERROR) << __func__
                << " Failed to create discovery snapshot for hit test";
    return XR_NULL_HANDLE;
  }

  // This is a temporary solution that is only safe because the current
  // implementation is synchronous. This should be replaced with a proper
  // synchronous API when it is available.
  // TODO(https://crbug.com/394772465)
  XrFuturePollInfoEXT poll_info = {XR_TYPE_FUTURE_POLL_INFO_EXT};
  poll_info.future = future;
  XrFuturePollResultEXT poll_result = {XR_TYPE_FUTURE_POLL_RESULT_EXT};
  while (poll_result.state != XR_FUTURE_STATE_READY_EXT) {
    if (XR_FAILED(extension_helper_->ExtensionMethods().xrPollFutureEXT(
            instance_, &poll_info, &poll_result))) {
      DLOG(ERROR) << __func__ << " Failed to poll future for hit test snapshot";
      return XR_NULL_HANDLE;
    }
  }

  XrCreateSpatialDiscoverySnapshotCompletionInfoEXT completion_info = {
      .type = XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_INFO_EXT,
      .baseSpace = mojo_space_,
      .time = predicted_display_time_,
      .future = future,
  };

  XrCreateSpatialDiscoverySnapshotCompletionEXT completion = {
      .type = XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_EXT};
  if (XR_FAILED(extension_helper_->ExtensionMethods()
                    .xrCreateSpatialDiscoverySnapshotCompleteEXT(
                        spatial_context, &completion_info, &completion))) {
    DLOG(ERROR) << __func__
                << " Failed to complete snapshot creation for hit test";
    return XR_NULL_HANDLE;
  }

  if (XR_FAILED(completion.futureResult)) {
    DLOG(ERROR) << __func__
                << " Hit test snapshot creation resulted in an error: "
                << std::hex << completion.futureResult;
    return XR_NULL_HANDLE;
  }

  return completion.snapshot;
}

std::vector<mojom::XRHitResultPtr> OpenXrSpatialHitTestManager::RequestHitTest(
    const gfx::Point3F& origin,
    const gfx::Vector3dF& direction) {
  // We're responsible for destroying the snapshot when we're done, since it's
  // one we've created.
  ScopedOpenXrObject<XrSpatialSnapshotEXT> snapshot(
      extension_helper_.get(), GetSnapshot(origin, direction));
  if (snapshot.get() == XR_NULL_HANDLE) {
    DLOG(ERROR) << __func__ << " Failed to extract snapshot";
    return {};
  }

  // Query hit result components.
  // Both plane and depth based results use the same component.
  std::array<XrSpatialComponentTypeEXT, 1> enabled_components = {
      XR_SPATIAL_COMPONENT_TYPE_RAYCAST_RESULT_ANDROID};
  XrSpatialComponentDataQueryConditionEXT query_condition = {
      .type = XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_CONDITION_EXT,
      .componentTypeCount = static_cast<uint32_t>(enabled_components.size()),
      .componentTypes = enabled_components.data(),
  };

  XrSpatialComponentDataQueryResultEXT query_result = {
      .type = XR_TYPE_SPATIAL_COMPONENT_DATA_QUERY_RESULT_EXT,
  };

  // Need to do an initial query to know how many results there are.
  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrQuerySpatialComponentDataEXT(
              snapshot.get(), &query_condition, &query_result))) {
    DLOG(ERROR) << __func__ << " Failed to query hit result components";
    return {};
  }

  std::vector<XrSpatialEntityIdEXT> entity_ids(
      query_result.entityIdCountOutput);
  query_result.entityIds = entity_ids.data();
  query_result.entityIdCapacityInput = entity_ids.size();

  std::vector<XrSpatialEntityTrackingStateEXT> entity_states(
      query_result.entityIdCountOutput);
  query_result.entityStates = entity_states.data();
  query_result.entityStateCapacityInput = entity_states.size();

  std::vector<XrSpatialRaycastResultANDROID> raycast_results(
      query_result.entityIdCountOutput);
  XrSpatialComponentRaycastResultListANDROID raycast_result_list = {
      .type = XR_TYPE_SPATIAL_COMPONENT_RAYCAST_RESULT_LIST_ANDROID,
      .raycastResultCount = static_cast<uint32_t>(raycast_results.size()),
      .raycastResults = raycast_results.data(),
  };

  query_result.next = &raycast_result_list;

  // Second query to populate all of the result data.
  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrQuerySpatialComponentDataEXT(
              snapshot.get(), &query_condition, &query_result))) {
    DLOG(ERROR) << __func__ << " Second query result call failed.";
    return {};
  }

  // The API essentially returns three main pieces of data back via three arrays
  // where data is guaranteed to be presented in the same order in each list.
  // However, when we return the results across mojom, we need the data to be
  // returned in ascending order of distance. Thus we create and then sort a
  // simple struct that keeps all of the bits of data together.
  std::vector<SpatialHitTestResult> sorted_results;
  sorted_results.reserve(raycast_results.size());
  for (size_t i = 0; i < raycast_results.size(); i++) {
    // If we get an entry that's not actively tracking, we don't want to return
    // that data, so just skip it.
    if (entity_states[i] != XR_SPATIAL_ENTITY_TRACKING_STATE_TRACKING_EXT) {
      continue;
    }

    // The `hitPose` provided to us returns a pose where +Z is the normal to the
    // hit surface, while WebXR wants +Y to be the normal to the hit surface.
    // Transform the `hitPose` now while building our results list.
    sorted_results.emplace_back(
        entity_ids[i],
        ZNormalXrPoseToYNormalDevicePose(raycast_results[i].hitPose),
        raycast_results[i].distanceSquared);
  }

  // WebXR expects to receive the hit poses in increasing distance from the item
  // that they hit, so sort them now.
  std::sort(sorted_results.begin(), sorted_results.end(),
            [](const SpatialHitTestResult& a, const SpatialHitTestResult& b) {
              return a.distance_squared < b.distance_squared;
            });

  std::vector<mojom::XRHitResultPtr> hit_results;
  hit_results.reserve(sorted_results.size());
  for (const auto& raycast_result : sorted_results) {
    DVLOG(3) << __func__ << "Id= " << raycast_result.id
             << " pose=" << raycast_result.pose.ToString();
    mojom::XRHitResultPtr hit_result = mojom::XRHitResult::New();
    hit_result->mojo_from_result = raycast_result.pose;
    // If the ID can't be found, we'll get an invalid plane_id to send up, which
    // is what we should be sending up for the other hit tests/the default value
    // anyways.
    if (plane_manager_ && raycast_result.id != XR_NULL_SPATIAL_ENTITY_ID_EXT) {
      hit_result->plane_id = plane_manager_->GetPlaneId(raycast_result.id);
    }

    hit_results.push_back(std::move(hit_result));
  }

  DVLOG(3) << __func__ << ": hit_results->size()=" << hit_results.size();
  return hit_results;
}

void OpenXrSpatialHitTestManager::OnStartProcessingHitTests(
    XrTime predicted_display_time) {
  predicted_display_time_ = predicted_display_time;
}

}  // namespace device
