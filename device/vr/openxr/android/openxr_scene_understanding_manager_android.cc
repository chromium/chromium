// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/android/openxr_scene_understanding_manager_android.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXRSceneUnderstandingManagerAndroid::OpenXRSceneUnderstandingManagerAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : mojo_space_(mojo_space) {
  const auto* extension_enum = extension_helper.ExtensionEnumeration();
  if (extension_enum->ExtensionSupported(
          XR_ANDROID_TRACKABLES_EXTENSION_NAME)) {
    anchor_manager_ = std::make_unique<OpenXrAnchorManagerAndroid>(
        extension_helper, session, mojo_space_);

    // For the time being, we only need the plane manager if we can perform
    // hit-tests.
    if (extension_enum->ExtensionSupported(XR_ANDROID_RAYCAST_EXTENSION_NAME)) {
      plane_manager_ = std::make_unique<OpenXrPlaneManagerAndroid>(
          extension_helper, session);
      hit_test_manager_ = std::make_unique<OpenXrHitTestManagerAndroid>(
          plane_manager_.get(), extension_helper, session, mojo_space_);
    }
  }
}

OpenXRSceneUnderstandingManagerAndroid::
    ~OpenXRSceneUnderstandingManagerAndroid() = default;

OpenXrSceneUnderstandingManagerType
OpenXRSceneUnderstandingManagerAndroid::GetType() const {
  return OpenXrSceneUnderstandingManagerType::kAndroid;
}

OpenXrPlaneManager* OpenXRSceneUnderstandingManagerAndroid::GetPlaneManager() {
  return plane_manager_.get();
}

OpenXrAnchorManager*
OpenXRSceneUnderstandingManagerAndroid::GetAnchorManager() {
  return anchor_manager_.get();
}

OpenXrHitTestManager*
OpenXRSceneUnderstandingManagerAndroid::GetHitTestManager() {
  return hit_test_manager_.get();
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
OpenXrSceneUnderstandingManagerAndroidFactory::GetSupportedFeatures() const {
  return supported_features_;
}

void OpenXrSceneUnderstandingManagerAndroidFactory::CheckAndUpdateEnabledState(
    const OpenXrExtensionEnumeration* extension_enum,
    XrInstance instance,
    XrSystemId system) {
  supported_features_.clear();
  if (extension_enum->ExtensionSupported(
          XR_ANDROID_TRACKABLES_EXTENSION_NAME)) {
    supported_features_.insert(device::mojom::XRSessionFeature::ANCHORS);

    // Hit Test needs Trackables and Raycast extensions.
    if (extension_enum->ExtensionSupported(XR_ANDROID_RAYCAST_EXTENSION_NAME)) {
      supported_features_.insert(device::mojom::XRSessionFeature::HIT_TEST);
    }
  }

  SetEnabled(!supported_features_.empty());
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrSceneUnderstandingManagerAndroidFactory::CreateSceneUnderstandingManager(
    const OpenXrExtensionHelper& extension_helper,
    OpenXrApiWrapper* openxr,
    XrSpace mojo_space) const {
  bool is_supported = IsEnabled();
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXRSceneUnderstandingManagerAndroid>(
        extension_helper, openxr->session(), mojo_space);
  }

  return nullptr;
}
}  // namespace device
