// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/msft/openxr_scene_understanding_manager_msft.h"

#include <algorithm>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/math_constants.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXRSceneUnderstandingManagerMSFT::OpenXRSceneUnderstandingManagerMSFT(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : mojo_space_(mojo_space) {
  const auto* extension_enum = extension_helper.ExtensionEnumeration();
  if (extension_enum->ExtensionSupported(
          XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME)) {
    anchor_manager_ = std::make_unique<OpenXrAnchorManagerMsft>(
        extension_helper, session, mojo_space_);
  }

  if (extension_enum->ExtensionSupported(
          XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME)) {
    plane_manager_ =
        std::make_unique<OpenXrPlaneManagerMsft>(extension_helper, session);
    hit_test_manager_ = std::make_unique<OpenXrHitTestManagerMsft>(
        plane_manager_.get(), mojo_space_);
  }
}

OpenXRSceneUnderstandingManagerMSFT::~OpenXRSceneUnderstandingManagerMSFT() =
    default;

OpenXrSceneUnderstandingManagerType
OpenXRSceneUnderstandingManagerMSFT::GetType() const {
  return OpenXrSceneUnderstandingManagerType::kMsft;
}

OpenXrPlaneManager* OpenXRSceneUnderstandingManagerMSFT::GetPlaneManager() {
  return plane_manager_.get();
}

OpenXrAnchorManager* OpenXRSceneUnderstandingManagerMSFT::GetAnchorManager() {
  return anchor_manager_.get();
}

OpenXrHitTestManager* OpenXRSceneUnderstandingManagerMSFT::GetHitTestManager() {
  return hit_test_manager_.get();
}

OpenXrSceneUnderstandingManagerMsftFactory::
    OpenXrSceneUnderstandingManagerMsftFactory() = default;
OpenXrSceneUnderstandingManagerMsftFactory::
    ~OpenXrSceneUnderstandingManagerMsftFactory() = default;

const base::flat_set<std::string_view>&
OpenXrSceneUnderstandingManagerMsftFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME,
       XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrSceneUnderstandingManagerMsftFactory::GetSupportedFeatures() const {
  return supported_features_;
}

void OpenXrSceneUnderstandingManagerMsftFactory::CheckAndUpdateEnabledState(
    const OpenXrExtensionEnumeration* extension_enum,
    XrInstance instance,
    XrSystemId system) {
  supported_features_.clear();

  if (extension_enum->ExtensionSupported(
          XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME)) {
    supported_features_.insert(device::mojom::XRSessionFeature::HIT_TEST);
  }

  if (extension_enum->ExtensionSupported(
          XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME)) {
    supported_features_.insert(device::mojom::XRSessionFeature::ANCHORS);
  }

  bool enabled = !supported_features_.empty();
  UMA_HISTOGRAM_BOOLEAN("XR.OpenXR.SceneUnderstandingMSFTAvailability",
                        enabled);
  SetEnabled(enabled);
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrSceneUnderstandingManagerMsftFactory::CreateSceneUnderstandingManager(
    const OpenXrExtensionHelper& extension_helper,
    OpenXrApiWrapper* openxr,
    XrSpace mojo_space) const {
  bool is_supported = IsEnabled();
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXRSceneUnderstandingManagerMSFT>(
        extension_helper, openxr->session(), mojo_space);
  }

  return nullptr;
}
}  // namespace device
