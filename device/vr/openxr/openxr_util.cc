// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_util.h"

#include <string>

#include "base/check_op.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace device {

XrPosef PoseIdentity() {
  XrPosef pose{};
  pose.orientation.w = 1;
  return pose;
}

gfx::Transform XrPoseToGfxTransform(const XrPosef& pose) {
  gfx::DecomposedTransform decomp;
  decomp.quaternion = gfx::Quaternion(pose.orientation.x, pose.orientation.y,
                                      pose.orientation.z, pose.orientation.w);
  decomp.translate[0] = pose.position.x;
  decomp.translate[1] = pose.position.y;
  decomp.translate[2] = pose.position.z;

  return gfx::Transform::Compose(decomp);
}

XrPosef GfxTransformToXrPose(const gfx::Transform& transform) {
  absl::optional<gfx::DecomposedTransform> decomposed_transform =
      transform.Decompose();
  // This pose should always be a simple translation and rotation so this should
  // always be true
  DCHECK(decomposed_transform);
  return {{static_cast<float>(decomposed_transform->quaternion.x()),
           static_cast<float>(decomposed_transform->quaternion.y()),
           static_cast<float>(decomposed_transform->quaternion.z()),
           static_cast<float>(decomposed_transform->quaternion.w())},
          {static_cast<float>(decomposed_transform->translate[0]),
           static_cast<float>(decomposed_transform->translate[1]),
           static_cast<float>(decomposed_transform->translate[2])}};
}

bool IsPoseValid(XrSpaceLocationFlags locationFlags) {
  XrSpaceLocationFlags PoseValidFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  return (locationFlags & PoseValidFlags) == PoseValidFlags;
}

XrResult GetSystem(XrInstance instance, XrSystemId* system) {
  XrSystemGetInfo system_info = {XR_TYPE_SYSTEM_GET_INFO};
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  return xrGetSystem(instance, &system_info, system);
}

#if BUILDFLAG(IS_WIN)
bool IsRunningInWin32AppContainer() {
  base::win::ScopedHandle scopedProcessToken;
  HANDLE processToken;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &processToken)) {
    return false;
  }

  scopedProcessToken.Set(processToken);

  BOOL isAppContainer;
  DWORD dwSize = sizeof(BOOL);
  if (!GetTokenInformation(scopedProcessToken.Get(), TokenIsAppContainer,
                           &isAppContainer, dwSize, &dwSize)) {
    return false;
  }

  return isAppContainer;
}
#else
bool IsRunningInWin32AppContainer() {
  return false;
}
#endif

XrResult CreateInstance(
    XrInstance* instance,
    const OpenXrExtensionEnumeration& extension_enumeration) {
  XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};

  std::string application_name = version_info::GetProductName() + " " +
                                 version_info::GetMajorVersionNumber();
  errno_t error =
      strcpy_s(instance_create_info.applicationInfo.applicationName,
               std::size(instance_create_info.applicationInfo.applicationName),
               application_name.c_str());
  DCHECK_EQ(error, 0);

  base::Version version = version_info::GetVersion();
  DCHECK_EQ(version.components().size(), 4uLL);
  uint32_t build = version.components()[2];

  // application version will be the build number of each vendor
  instance_create_info.applicationInfo.applicationVersion = build;

  error = strcpy_s(instance_create_info.applicationInfo.engineName,
                   std::size(instance_create_info.applicationInfo.engineName),
                   "Chromium");
  DCHECK_EQ(error, 0);

  // engine version should be the build number of chromium
  instance_create_info.applicationInfo.engineVersion = build;

  instance_create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

  // xrCreateInstance validates the list of extensions and returns
  // XR_ERROR_EXTENSION_NOT_PRESENT if an extension is not supported,
  // so we don't need to call xrEnumerateInstanceExtensionProperties
  // to validate these extensions.
  // Since the OpenXR backend only knows how to draw with D3D11 at the moment,
  // the XR_KHR_D3D11_ENABLE_EXTENSION_NAME is required.
  std::vector<const char*> extensions{XR_KHR_D3D11_ENABLE_EXTENSION_NAME};

  // If we are in an app container, we must require the app container extension
  // to ensure robust execution of the OpenXR runtime
  if (IsRunningInWin32AppContainer()) {
    // Add the win32 app container compatible extension to our list of
    // extensions. If this runtime does not support execution in an app
    // container environment, one of xrCreateInstance or xrGetSystem will fail.
    extensions.push_back(XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME);
  }

  auto EnableExtensionIfSupported = [&extension_enumeration,
                                     &extensions](const char* extension) {
    if (extension_enumeration.ExtensionSupported(extension)) {
      extensions.push_back(extension);
    }
  };

  // XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME, is required for optional
  // functionality (unbounded reference spaces) and thus only requested if it is
  // available.
  EnableExtensionIfSupported(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME);

  // Input extensions. These enable interaction profiles not defined in the core
  // spec
  EnableExtensionIfSupported(kExtSamsungOdysseyControllerExtensionName);
  EnableExtensionIfSupported(kExtHPMixedRealityControllerExtensionName);
  EnableExtensionIfSupported(kMSFTHandInteractionExtensionName);
  EnableExtensionIfSupported(XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME);

  EnableExtensionIfSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
  EnableExtensionIfSupported(XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
  EnableExtensionIfSupported(XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME);

  EnableExtensionIfSupported(
      XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME);
  if (extension_enumeration.ExtensionSupported(
          XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME)) {
    EnableExtensionIfSupported(XR_MSFT_FIRST_PERSON_OBSERVER_EXTENSION_NAME);
  }

  EnableExtensionIfSupported(
      XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME);

  instance_create_info.enabledExtensionCount =
      static_cast<uint32_t>(extensions.size());
  instance_create_info.enabledExtensionNames = extensions.data();

  return xrCreateInstance(&instance_create_info, instance);
}

std::vector<XrEnvironmentBlendMode> GetSupportedBlendModes(XrInstance instance,
                                                           XrSystemId system) {
  // Query the list of supported environment blend modes for the current system.
  uint32_t blend_mode_count;
  const XrViewConfigurationType kSupportedViewConfiguration =
      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  if (XR_FAILED(xrEnumerateEnvironmentBlendModes(instance, system,
                                                 kSupportedViewConfiguration, 0,
                                                 &blend_mode_count, nullptr)))
    return {};  // empty vector

  std::vector<XrEnvironmentBlendMode> environment_blend_modes(blend_mode_count);
  if (XR_FAILED(xrEnumerateEnvironmentBlendModes(
          instance, system, kSupportedViewConfiguration, blend_mode_count,
          &blend_mode_count, environment_blend_modes.data())))
    return {};  // empty vector

  return environment_blend_modes;
}

}  // namespace device
