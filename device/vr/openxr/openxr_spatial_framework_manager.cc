// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_framework_manager.h"

#include <set>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_spatial_anchor_manager.h"
#include "device/vr/openxr/openxr_spatial_capability_configuration_base.h"
#include "device/vr/openxr/openxr_spatial_hit_test_manager.h"
#include "device/vr/openxr/openxr_spatial_plane_manager.h"
#include "device/vr/openxr/openxr_spatial_utils.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

#define OPENXR_LOAD_FN(fn)             \
  PFN_##fn fn = nullptr;               \
  std::ignore = xrGetInstanceProcAddr( \
      instance, #fn, reinterpret_cast<PFN_xrVoidFunction*>(&fn));

namespace device {

OpenXrSpatialFrameworkManager::OpenXrSpatialFrameworkManager(
    const OpenXrExtensionHelper& extension_helper,
    OpenXrApiWrapper* openxr,
    XrSpace space,
    const std::set<device::mojom::XRSessionFeature>& supported_features)
    : extension_helper_(extension_helper),
      openxr_(openxr),
      base_space_(space) {
  absl::flat_hash_map<XrSpatialCapabilityEXT,
                      absl::flat_hash_set<XrSpatialComponentTypeEXT>>
      capability_configuration;
  if (supported_features.contains(
          device::mojom::XRSessionFeature::PLANE_DETECTION)) {
    plane_manager_ = std::make_unique<OpenXrSpatialPlaneManager>(
        base_space_, extension_helper_.get(), *this, openxr_->instance(),
        openxr_->system());
    plane_manager_->PopulateCapabilityConfiguration(capability_configuration);
  }

  if (supported_features.contains(device::mojom::XRSessionFeature::HIT_TEST)) {
    hit_test_manager_ = std::make_unique<OpenXrSpatialHitTestManager>(
        extension_helper_.get(), *this, plane_manager_.get(), base_space_,
        openxr_->instance(), openxr_->system());
    hit_test_manager_->PopulateCapabilityConfiguration(
        capability_configuration);
  }

  if (supported_features.contains(device::mojom::XRSessionFeature::ANCHORS)) {
    anchor_manager_ = std::make_unique<OpenXrSpatialAnchorManager>(
        extension_helper_.get(), *this, plane_manager_.get(), base_space_);
    anchor_manager_->PopulateCapabilityConfiguration(capability_configuration);
  }

  // Even though we are kicking off an asynchronous OpenXr function, the
  // passed-in create_info and it's children (including the list of capability
  // configs), do not need to live until the future is completed. They will
  // be copied by the runtime. We make our own intermediate helper class just
  // to help abstract some of the details of creating the child structs, even
  // though at present we only have a configuration base.
  std::vector<OpenXrSpatialCapabilityConfigurationBase> capability_configs;
  std::vector<XrSpatialCapabilityConfigurationBaseHeaderEXT*>
      capability_config_ptrs;
  for (auto& [capability, components] : capability_configuration) {
    capability_configs.emplace_back(capability, components);
    capability_config_ptrs.push_back(
        capability_configs.back().GetAsBaseHeader());
  }

  XrSpatialContextCreateInfoEXT create_info = {
      XR_TYPE_SPATIAL_CONTEXT_CREATE_INFO_EXT};
  create_info.capabilityConfigCount =
      static_cast<uint32_t>(capability_config_ptrs.size());
  create_info.capabilityConfigs = capability_config_ptrs.data();

  XrFutureEXT future;
  // Failure at this point indicates that either the create_info was built wrong
  // or something is seriously wrong with the system.
  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrCreateSpatialContextAsyncEXT(
              openxr_->session(), &create_info, &future))) {
    DLOG(ERROR) << __func__ << " Failed to create spatial context";
    return;
  }
  openxr_->PollFuture(
      future,
      base::BindOnce(
          &OpenXrSpatialFrameworkManager::OnCreateSpatialContextComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

OpenXrSpatialFrameworkManager::~OpenXrSpatialFrameworkManager() {
  if (discovery_snapshot_ != XR_NULL_HANDLE) {
    extension_helper_->ExtensionMethods().xrDestroySpatialSnapshotEXT(
        discovery_snapshot_);
  }

  if (spatial_context_ != XR_NULL_HANDLE) {
    extension_helper_->ExtensionMethods().xrDestroySpatialContextEXT(
        spatial_context_);
  }
}

OpenXrSceneUnderstandingManagerType OpenXrSpatialFrameworkManager::GetType()
    const {
  return OpenXrSceneUnderstandingManagerType::kSpatialEntities;
}

OpenXrPlaneManager* OpenXrSpatialFrameworkManager::GetPlaneManager() {
  return plane_manager_.get();
}

OpenXrHitTestManager* OpenXrSpatialFrameworkManager::GetHitTestManager() {
  return hit_test_manager_.get();
}

OpenXrAnchorManager* OpenXrSpatialFrameworkManager::GetAnchorManager() {
  return anchor_manager_.get();
}

void OpenXrSpatialFrameworkManager::OnDiscoveryRecommended(
    const XrEventDataSpatialDiscoveryRecommendedEXT* event_data) {
  if (spatial_context_ == XR_NULL_HANDLE ||
      event_data->spatialContext != spatial_context_) {
    return;
  }

  // Per the spec, we can query for all components we've enabled on the spatial
  // context by leaving the component types empty field of this create info
  // empty.
  XrSpatialDiscoverySnapshotCreateInfoEXT snapshot_create_info = {
      XR_TYPE_SPATIAL_DISCOVERY_SNAPSHOT_CREATE_INFO_EXT};
  XrFutureEXT future;
  // Failure at this point indicates that either the create_info was built wrong
  // or something is seriously wrong with the system.
  if (XR_FAILED(extension_helper_->ExtensionMethods()
                    .xrCreateSpatialDiscoverySnapshotAsyncEXT(
                        spatial_context_, &snapshot_create_info, &future))) {
    DLOG(ERROR) << __func__ << " Failed to create discovery snapshot";
    return;
  }

  openxr_->PollFuture(
      future, base::BindOnce(&OpenXrSpatialFrameworkManager::
                                 OnCreateSpatialDiscoverySnapshotComplete,
                             weak_ptr_factory_.GetWeakPtr()));
}

XrSpatialContextEXT OpenXrSpatialFrameworkManager::GetSpatialContext() const {
  return spatial_context_;
}

XrSpatialSnapshotEXT OpenXrSpatialFrameworkManager::GetDiscoverySnapshot()
    const {
  return discovery_snapshot_;
}

void OpenXrSpatialFrameworkManager::OnCreateSpatialContextComplete(
    XrFutureEXT future) {
  DVLOG(2) << __func__;
  if (future == XR_NULL_FUTURE_EXT) {
    DLOG(ERROR) << __func__ << " Received invalid future";
    return;
  }

  XrCreateSpatialContextCompletionEXT complete_info = {
      XR_TYPE_CREATE_SPATIAL_CONTEXT_COMPLETION_EXT};
  if (XR_FAILED(extension_helper_->ExtensionMethods()
                    .xrCreateSpatialContextCompleteEXT(
                        openxr_->session(), future, &complete_info))) {
    return;
  }

  if (XR_FAILED(complete_info.futureResult)) {
    return;
  }

  spatial_context_ = complete_info.spatialContext;
}

void OpenXrSpatialFrameworkManager::OnCreateSpatialDiscoverySnapshotComplete(
    XrFutureEXT future) {
  DVLOG(2) << __func__;
  if (future == XR_NULL_FUTURE_EXT) {
    DLOG(ERROR) << __func__ << " Received invalid future";
    return;
  }

  XrCreateSpatialDiscoverySnapshotCompletionInfoEXT completion_info = {
      XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_INFO_EXT};
  completion_info.baseSpace = base_space_;
  completion_info.future = future;
  // Because we are doing this outside of a frame loop, just grab the latest
  // available predicted display time. This snapshot only updates periodically
  // at best anyways, and the more accurate information is queried using this
  // snapshot, but with the more up to date predicted display time.
  completion_info.time = openxr_->GetPredictedDisplayTime();

  XrCreateSpatialDiscoverySnapshotCompletionEXT completion = {
      XR_TYPE_CREATE_SPATIAL_DISCOVERY_SNAPSHOT_COMPLETION_EXT};

  if (XR_FAILED(extension_helper_->ExtensionMethods()
                    .xrCreateSpatialDiscoverySnapshotCompleteEXT(
                        spatial_context_, &completion_info, &completion))) {
    DLOG(ERROR) << __func__ << " Failed to complete snapshot creation";
    return;
  }

  if (XR_FAILED(completion.futureResult)) {
    DLOG(ERROR) << __func__
                << " Snapshot creation resulted in an error: " << std::hex
                << completion.futureResult;
    return;
  }

  if (discovery_snapshot_ != XR_NULL_HANDLE) {
    extension_helper_->ExtensionMethods().xrDestroySpatialSnapshotEXT(
        discovery_snapshot_);
  }

  discovery_snapshot_ = completion.snapshot;

  if (plane_manager_) {
    plane_manager_->OnSnapshotChanged();
  }
}

OpenXrSpatialFrameworkManagerFactory::OpenXrSpatialFrameworkManagerFactory() =
    default;

OpenXrSpatialFrameworkManagerFactory::~OpenXrSpatialFrameworkManagerFactory() =
    default;

const base::flat_set<std::string_view>&
OpenXrSpatialFrameworkManagerFactory::GetRequestedExtensions() const {
  if (!base::FeatureList::IsEnabled(features::kOpenXrSpatialEntities)) {
    static base::NoDestructor<base::flat_set<std::string_view>> kEmptySet({});
    return *kEmptySet;
  }

  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions({
      XR_EXT_FUTURE_EXTENSION_NAME,
      XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME,
      XR_EXT_SPATIAL_ANCHOR_EXTENSION_NAME,
      XR_EXT_SPATIAL_PLANE_TRACKING_EXTENSION_NAME,
      XR_ANDROID_SPATIAL_DISCOVERY_RAYCAST_EXTENSION_NAME,
      XR_ANDROID_SPATIAL_ENTITY_BOUND_ANCHOR_EXTENSION_NAME,
  });

  return *kExtensions;
}

void OpenXrSpatialFrameworkManagerFactory::CheckAndUpdateEnabledState(
    const OpenXrExtensionEnumeration* extension_enum,
    XrInstance instance,
    XrSystemId system_id) {
  supported_features_.clear();
  SetEnabled(false);

  if (!base::FeatureList::IsEnabled(features::kOpenXrSpatialEntities)) {
    return;
  }

  if (!extension_enum->ExtensionSupported(
          XR_EXT_SPATIAL_ENTITY_EXTENSION_NAME)) {
    return;
  }

  OPENXR_LOAD_FN(xrEnumerateSpatialCapabilitiesEXT);
  OPENXR_LOAD_FN(xrEnumerateSpatialCapabilityComponentTypesEXT);

  if (xrEnumerateSpatialCapabilitiesEXT == nullptr ||
      xrEnumerateSpatialCapabilityComponentTypesEXT == nullptr) {
    return;
  }

  std::vector<XrSpatialCapabilityEXT> capabilities =
      GetCapabilities(xrEnumerateSpatialCapabilitiesEXT, instance, system_id);
  if (extension_enum->ExtensionSupported(
          XR_EXT_SPATIAL_PLANE_TRACKING_EXTENSION_NAME) &&
      OpenXrSpatialPlaneManager::IsSupported(capabilities)) {
    supported_features_.insert(
        device::mojom::XRSessionFeature::PLANE_DETECTION);
  }

  if (extension_enum->ExtensionSupported(
          XR_EXT_SPATIAL_ANCHOR_EXTENSION_NAME) &&
      OpenXrSpatialAnchorManager::IsSupported(capabilities)) {
    supported_features_.insert(device::mojom::XRSessionFeature::ANCHORS);
  }

  if (extension_enum->ExtensionSupported(
          XR_ANDROID_SPATIAL_DISCOVERY_RAYCAST_EXTENSION_NAME)) {
    if (OpenXrSpatialHitTestManager::IsSupported(
            instance, system_id, xrEnumerateSpatialCapabilityComponentTypesEXT,
            capabilities)) {
      supported_features_.insert(device::mojom::XRSessionFeature::HIT_TEST);
    }
  }

  SetEnabled(!supported_features_.empty());
}

std::set<device::mojom::XRSessionFeature>
OpenXrSpatialFrameworkManagerFactory::GetSupportedFeatures() const {
  return supported_features_;
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrSpatialFrameworkManagerFactory::CreateSceneUnderstandingManager(
    const OpenXrExtensionHelper& extension_helper,
    OpenXrApiWrapper* openxr,
    XrSpace base_space) const {
  bool is_supported = IsEnabled();
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrSpatialFrameworkManager>(
        extension_helper, openxr, base_space, supported_features_);
  }

  return nullptr;
}

}  // namespace device
