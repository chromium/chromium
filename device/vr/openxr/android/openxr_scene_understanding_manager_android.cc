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
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXRSceneUnderstandingManagerAndroid::OpenXRSceneUnderstandingManagerAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper), mojo_space_(mojo_space) {
  plane_manager_ =
      std::make_unique<OpenXrPlaneManagerAndroid>(extension_helper, session);
  hit_test_manager_ = std::make_unique<OpenXrHitTestManagerAndroid>(
      plane_manager_.get(), extension_helper, session, mojo_space_);
}

OpenXRSceneUnderstandingManagerAndroid::
    ~OpenXRSceneUnderstandingManagerAndroid() = default;

OpenXrPlaneManager* OpenXRSceneUnderstandingManagerAndroid::GetPlaneManager() {
  return plane_manager_.get();
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
