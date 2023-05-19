// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/openxr_platform_helper.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "device/vr/openxr/openxr_defs.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_graphics_binding.h"

namespace device {

OpenXrPlatformHelper::OpenXrPlatformHelper() = default;
OpenXrPlatformHelper::~OpenXrPlatformHelper() = default;

bool OpenXrPlatformHelper::EnsureInitialized() {
  if (initialized_) {
    return true;
  }

  if (!Initialize()) {
    return false;
  }

  // The subclass may have already created the extension enumeration.
  if (!extension_enumeration_) {
    extension_enumeration_ = std::make_unique<OpenXrExtensionEnumeration>();
  }

  initialized_ = true;
  return true;
}

// Gets the ExtensionEnumeration which is the list of extensions supported by
// the platform.
const OpenXrExtensionEnumeration*
OpenXrPlatformHelper::GetExtensionEnumeration() const {
  CHECK(initialized_);
  return extension_enumeration_.get();
}

XrResult OpenXrPlatformHelper::CreateInstance(XrInstance* instance) {
  return CreateInstance(instance, absl::nullopt);
}

XrResult OpenXrPlatformHelper::CreateInstance(
    XrInstance* instance,
    absl::optional<OpenXrCreateInfo> create_info) {
  CHECK(initialized_);
  CHECK(xr_instance_ == XR_NULL_HANDLE)
      << "Each Process is only allowed one XrInstance at a time";
  XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};

  std::string application_name =
      base::StrCat({version_info::GetProductName(), " ",
                    version_info::GetMajorVersionNumber()});
  size_t dest_size =
      std::size(instance_create_info.applicationInfo.applicationName);
  size_t src_size =
      base::strlcpy(instance_create_info.applicationInfo.applicationName,
                    application_name.c_str(), dest_size);
  DCHECK_LT(src_size, dest_size);

  base::Version version = version_info::GetVersion();
  DCHECK_EQ(version.components().size(), 4uLL);
  uint32_t build = version.components()[2];

  // application version will be the build number of each vendor
  instance_create_info.applicationInfo.applicationVersion = build;

  dest_size = std::size(instance_create_info.applicationInfo.engineName);
  src_size = base::strlcpy(instance_create_info.applicationInfo.engineName,
                           "Chromium", dest_size);
  DCHECK_LT(src_size, dest_size);

  // engine version should be the build number of chromium
  instance_create_info.applicationInfo.engineVersion = build;

  instance_create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

  // xrCreateInstance validates the list of extensions and returns
  // XR_ERROR_EXTENSION_NOT_PRESENT if an extension is not supported,
  // so we don't need to call xrEnumerateInstanceExtensionProperties
  // to validate these extensions.
  std::vector<const char*> extensions;
  GetRequiredExtensions(extensions);
  OpenXrGraphicsBinding::GetRequiredExtensions(extensions);

  // Create a local variable for the lambda to capture. This is okay since we
  // aren't passing the lambda anywhere.
  auto* extension_enumeration = GetExtensionEnumeration();
  auto EnableExtensionIfSupported = [&extension_enumeration,
                                     &extensions](const char* extension) {
    if (extension_enumeration->ExtensionSupported(extension)) {
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
  EnableExtensionIfSupported(
      XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME);

  EnableExtensionIfSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
  EnableExtensionIfSupported(XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
  EnableExtensionIfSupported(XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME);

  EnableExtensionIfSupported(
      XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME);
  if (GetExtensionEnumeration()->ExtensionSupported(
          XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME)) {
    EnableExtensionIfSupported(XR_MSFT_FIRST_PERSON_OBSERVER_EXTENSION_NAME);
  }

  // Enable any other platform-specific extensions that we don't just enable or
  // try to enable across the board.
  for (const auto* extension : GetOptionalExtensions()) {
    EnableExtensionIfSupported(extension);
  }

  instance_create_info.enabledExtensionCount =
      static_cast<uint32_t>(extensions.size());
  instance_create_info.enabledExtensionNames = extensions.data();

  if (create_info.has_value()) {
    instance_create_info.next = GetPlatformCreateInfo(create_info.value());
  } else if (BUILDFLAG(IS_ANDROID)) {
    LOG(ERROR) << "Android was missing CreateInfo";
  }

  XrResult result = xrCreateInstance(&instance_create_info, instance);
  if (XR_SUCCEEDED(result)) {
    xr_instance_ = *instance;
  }

  return result;
}

XrResult OpenXrPlatformHelper::DestroyInstance(XrInstance& instance) {
  CHECK(instance != XR_NULL_HANDLE);
  CHECK(instance == xr_instance_);
  XrResult result = xrDestroyInstance(instance);
  if (XR_SUCCEEDED(result)) {
    instance = XR_NULL_HANDLE;
    xr_instance_ = XR_NULL_HANDLE;
  }
  return result;
}

}  // namespace device
