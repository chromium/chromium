// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_extension_helper.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_extension_handler_factories.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/openxr/openxr_platform_helper.h"
#include "device/vr/public/mojom/xr_session.mojom.h"

namespace device {

namespace {
// A helper macro to avoid the repetitive boilerplate required to load a
// function from OpenXR. Note that these are typically extension functions and
// that the name of the method is repeated several times throughout the
// declaration. For a function named "xrCreateHandTrackerEXT" this will exapnd
// to the following:
// std::ignore = xrGetInstanceProcAddr(
//     instance, "xrCreateHandTrackerEXT",
//     reinterpret_cast<PFN_xrVoidFunction*>(
//         const_cast<PFN_xrCreateHandTrackerEXT*>(
//             &extension_methods_.xrCreateHandTrackerEXT)));
#define OPENXR_LOAD_FN(name)                 \
  std::ignore = xrGetInstanceProcAddr(       \
      instance, #name,                       \
      reinterpret_cast<PFN_xrVoidFunction*>( \
          const_cast<PFN_##name*>(&extension_methods_.name)))

// The ExtensionHandlers are all created/checked in very similar ways, with the
// only difference really being what method they need called on the
// OpenXrExtensionHandler factory to be created (and thus the arguments that
// they need to have supplied). This helper abstracts the boilerplate so that
// we can simply use a factory lambda which has that knowledge, and this helper
// then takes care of the rest of the boilerplate.
template <typename T, typename FunctionType>
std::unique_ptr<T> CreateExtensionHandler(
    FunctionType fn) {
  for (const auto* factory : GetExtensionHandlerFactories()) {
    CHECK(factory);
    if (factory->IsEnabled()) {
      auto ret = fn(*factory);
      if (ret != nullptr) {
        return ret;
      }
    }
  }

  return nullptr;
}

bool IsSceneUnderstandingFeature(mojom::XRSessionFeature feature) {
  return feature == device::mojom::XRSessionFeature::ANCHORS ||
         feature == device::mojom::XRSessionFeature::HIT_TEST ||
         feature == device::mojom::XRSessionFeature::PLANE_DETECTION;
}
}  // namespace

OpenXrExtensionMethods::OpenXrExtensionMethods() = default;
OpenXrExtensionMethods::~OpenXrExtensionMethods() = default;

OpenXrExtensionEnumeration::OpenXrExtensionEnumeration() {
  uint32_t extension_count;
  if (XR_SUCCEEDED(xrEnumerateInstanceExtensionProperties(
          nullptr, 0, &extension_count, nullptr))) {
    extension_properties_.resize(extension_count,
                                 {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, extension_count,
                                           &extension_count,
                                           extension_properties_.data());
  }

  if constexpr (DCHECK_IS_ON()) {
    DVLOG(1) << __func__ << ": Supported Extensions Begin";
    for (const auto& extension : extension_properties_) {
      DVLOG(1) << __func__ << ": " << extension.extensionName
               << " version=" << extension.extensionVersion;
    }
    DVLOG(1) << __func__ << ": Supported Extensions End";
  }
}

OpenXrExtensionEnumeration::~OpenXrExtensionEnumeration() = default;

bool OpenXrExtensionEnumeration::ExtensionSupported(
    std::string_view extension_name) const {
  return std::ranges::any_of(
      extension_properties_,
      [&extension_name](const XrExtensionProperties& properties) {
        return std::string_view(properties.extensionName) == extension_name;
      });
}

// static
std::vector<const char*>
OpenXrExtensionHelper::GetRequiredExtensionsForLayers() {
  return {XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME,
          XR_KHR_COMPOSITION_LAYER_EQUIRECT2_EXTENSION_NAME,
          XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME};
}

OpenXrExtensionHelper::~OpenXrExtensionHelper() = default;

OpenXrExtensionHelper::OpenXrExtensionHelper(
    XrInstance instance,
    const OpenXrExtensionEnumeration* const extension_enumeration)
    : extension_enumeration_(extension_enumeration) {
  // Failure to query a method results in a nullptr

  // General methods
  OPENXR_LOAD_FN(xrPollFutureEXT);

  // Hand tracking methods
  OPENXR_LOAD_FN(xrCreateHandTrackerEXT);
  OPENXR_LOAD_FN(xrDestroyHandTrackerEXT);
  OPENXR_LOAD_FN(xrLocateHandJointsEXT);

  // Anchors methods
  OPENXR_LOAD_FN(xrCreateSpatialAnchorMSFT);
  OPENXR_LOAD_FN(xrDestroySpatialAnchorMSFT);
  OPENXR_LOAD_FN(xrCreateSpatialAnchorSpaceMSFT);

  // MSFT Scene Understanding Methods
  OPENXR_LOAD_FN(xrEnumerateSceneComputeFeaturesMSFT);
  OPENXR_LOAD_FN(xrCreateSceneObserverMSFT);
  OPENXR_LOAD_FN(xrDestroySceneObserverMSFT);
  OPENXR_LOAD_FN(xrCreateSceneMSFT);
  OPENXR_LOAD_FN(xrDestroySceneMSFT);
  OPENXR_LOAD_FN(xrComputeNewSceneMSFT);
  OPENXR_LOAD_FN(xrGetSceneComputeStateMSFT);
  OPENXR_LOAD_FN(xrGetSceneComponentsMSFT);
  OPENXR_LOAD_FN(xrLocateSceneComponentsMSFT);
  OPENXR_LOAD_FN(xrGetSceneMeshBuffersMSFT);

  // Spatial Entities
  OPENXR_LOAD_FN(xrCreateSpatialContextAsyncEXT);
  OPENXR_LOAD_FN(xrCreateSpatialContextCompleteEXT);
  OPENXR_LOAD_FN(xrCreateSpatialDiscoverySnapshotAsyncEXT);
  OPENXR_LOAD_FN(xrCreateSpatialDiscoverySnapshotCompleteEXT);
  OPENXR_LOAD_FN(xrCreateSpatialUpdateSnapshotEXT);
  OPENXR_LOAD_FN(xrDestroySpatialContextEXT);
  OPENXR_LOAD_FN(xrDestroySpatialEntityEXT);
  OPENXR_LOAD_FN(xrDestroySpatialSnapshotEXT);
  OPENXR_LOAD_FN(xrEnumerateSpatialCapabilitiesEXT);
  OPENXR_LOAD_FN(xrEnumerateSpatialCapabilityComponentTypesEXT);
  OPENXR_LOAD_FN(xrQuerySpatialComponentDataEXT);

  // Spatial Anchors
  OPENXR_LOAD_FN(xrCreateSpatialAnchorEXT);
  OPENXR_LOAD_FN(xrEnumerateSpatialAnchorAttachableComponentsANDROID);

  // Visibility Mask
  OPENXR_LOAD_FN(xrGetVisibilityMaskKHR);

#if BUILDFLAG(IS_WIN)
  OPENXR_LOAD_FN(xrConvertWin32PerformanceCounterToTimeKHR);
#endif

#if BUILDFLAG(IS_ANDROID)
  OPENXR_LOAD_FN(xrCreateTrackableTrackerANDROID);
  OPENXR_LOAD_FN(xrDestroyTrackableTrackerANDROID);

  OPENXR_LOAD_FN(xrRaycastANDROID);

  OPENXR_LOAD_FN(xrCreateAnchorSpaceANDROID);

  OPENXR_LOAD_FN(xrCreateLightEstimatorANDROID);
  OPENXR_LOAD_FN(xrDestroyLightEstimatorANDROID);
  OPENXR_LOAD_FN(xrGetLightEstimateANDROID);

  OPENXR_LOAD_FN(xrCreateDepthSwapchainANDROID);
  OPENXR_LOAD_FN(xrDestroyDepthSwapchainANDROID);
  OPENXR_LOAD_FN(xrEnumerateDepthSwapchainImagesANDROID);
  OPENXR_LOAD_FN(xrEnumerateDepthResolutionsANDROID);
  OPENXR_LOAD_FN(xrAcquireDepthSwapchainImagesANDROID);
#endif
}

bool OpenXrExtensionHelper::IsFeatureSupported(
    device::mojom::XRSessionFeature feature) const {
  switch (feature) {
    case device::mojom::XRSessionFeature::ANCHORS:
    case device::mojom::XRSessionFeature::DEPTH:
    case device::mojom::XRSessionFeature::HAND_INPUT:
    case device::mojom::XRSessionFeature::HIT_TEST:
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
      return std::ranges::any_of(
          GetExtensionHandlerFactories(),
          [feature](const auto* extension_handler_factory) {
            return base::Contains(
                extension_handler_factory->GetSupportedFeatures(), feature);
          });
    case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
      return IsExtensionSupported(
          XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME);
    case device::mojom::XRSessionFeature::LAYERS:
      return std::ranges::all_of(GetRequiredExtensionsForLayers(),
                                 [this](const char* extension) {
                                   return IsExtensionSupported(extension);
                                 });
    default:
      // By default we assume a feature doesn't need to be supported by an
      // extension unless customized above.
      return true;
  }
}

bool OpenXrExtensionHelper::IsExtensionSupported(
    const char* extension_name) const {
  return extension_enumeration_->ExtensionSupported(extension_name);
}

std::unique_ptr<OpenXrDepthSensor> OpenXrExtensionHelper::CreateDepthSensor(
    XrSession session,
    XrSpace base_space,
    const mojom::XRDepthOptions& depth_options) const {
  return CreateExtensionHandler<OpenXrDepthSensor>(
      [this, session, base_space,
       depth_options](const OpenXrExtensionHandlerFactory& factory)
          -> std::unique_ptr<OpenXrDepthSensor> {
        auto sensor = factory.CreateDepthSensor(*this, session, base_space,
                                                depth_options);
        if (sensor && XR_SUCCEEDED(sensor->Initialize())) {
          return sensor;
        }

        return nullptr;
      });
}

std::unique_ptr<OpenXrHandTracker> OpenXrExtensionHelper::CreateHandTracker(
    XrSession session,
    OpenXrHandednessType handedness) const {
  return CreateExtensionHandler<OpenXrHandTracker>(
      [this, session,
       handedness](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateHandTracker(*this, session, handedness);
      });
}

std::unique_ptr<OpenXrLightEstimator>
OpenXrExtensionHelper::CreateLightEstimator(XrSession session,
                                            XrSpace base_space) const {
  return CreateExtensionHandler<OpenXrLightEstimator>(
      [this, session,
       base_space](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateLightEstimator(*this, session, base_space);
      });
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrExtensionHelper::CreateSceneUnderstandingManager(
    OpenXrApiWrapper* openxr,
    XrSpace base_space,
    const std::vector<mojom::XRSessionFeature>& required_features,
    const std::vector<mojom::XRSessionFeature>& optional_features) const {
  DVLOG(1) << __func__;
  const OpenXrExtensionHandlerFactory* best_factory = nullptr;
  size_t best_supported_optional_features_count = 0;

  for (const auto* factory : GetExtensionHandlerFactories()) {
    CHECK(factory);
    if (!factory->IsEnabled()) {
      continue;
    }

    const auto supported_features = factory->GetSupportedFeatures();

    auto supported_function =
        [&supported_features](mojom::XRSessionFeature feature) {
          return IsSceneUnderstandingFeature(feature) &&
                 base::Contains(supported_features, feature);
        };

    // Get the count of how many required and optional features are scene
    // understanding features.
    size_t required_features_requested_count =
        std::ranges::count_if(required_features, &IsSceneUnderstandingFeature);
    size_t optional_features_requested_count =
        std::ranges::count_if(optional_features, &IsSceneUnderstandingFeature);
    CHECK(required_features_requested_count > 0 ||
          optional_features_requested_count > 0)
        << "Requested a SceneUnderstandingManager, but no "
           "SceneUnderstandingManager features are requested";

    // Now, see how many of our supported features are scene understanding
    // features.
    size_t supported_required_features_count =
        std::ranges::count_if(required_features, supported_function);
    size_t supported_optional_features_count =
        std::ranges::count_if(optional_features, supported_function);

    // If all required features are not supported, we can't use this factory.
    if (supported_required_features_count !=
        required_features_requested_count) {
      continue;
    }

    // If this SceneUnderstandingManager supports all of the optional features,
    // then use it.
    if (supported_optional_features_count ==
        optional_features_requested_count) {
      return factory->CreateSceneUnderstandingManager(*this, openxr,
                                                      base_space);
    }

    // Otherwise, if this factory supports more optional features than our
    // current best choice, update the best count/factory and keep going.
    if (supported_optional_features_count >
        best_supported_optional_features_count) {
      best_supported_optional_features_count =
          supported_required_features_count;
      best_factory = factory;
    }
  }

  std::unique_ptr<OpenXRSceneUnderstandingManager> manager;
  if (best_factory) {
    manager = best_factory->CreateSceneUnderstandingManager(*this, openxr,
                                                            base_space);
  }

  UMA_HISTOGRAM_ENUMERATION("XR.OpenXR.SceneUnderstandingManagerType",
                            manager
                                ? manager->GetType()
                                : OpenXrSceneUnderstandingManagerType::kNone);

  return manager;
}

std::unique_ptr<OpenXrStageBoundsProvider>
OpenXrExtensionHelper::CreateStageBoundsProvider(XrSession session) const {
  return CreateExtensionHandler<OpenXrStageBoundsProvider>(
      [session](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateStageBoundsProvider(session);
      });
}

std::unique_ptr<OpenXrUnboundedSpaceProvider>
OpenXrExtensionHelper::CreateUnboundedSpaceProvider() const {
  return CreateExtensionHandler<OpenXrUnboundedSpaceProvider>(
      [](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateUnboundedSpaceProvider();
      });
}

}  // namespace device
