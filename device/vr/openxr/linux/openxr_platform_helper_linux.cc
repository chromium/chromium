// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/linux/openxr_platform_helper_linux.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "device/vr/openxr/linux/openxr_graphics_binding_vulkan.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// static
void OpenXrPlatformHelper::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
  // Linux has no platform-specific required extensions for xrCreateInstance.
}

// static
std::vector<const char*> OpenXrPlatformHelper::GetOptionalExtensions() {
  return {};
}

OpenXrPlatformHelperLinux::OpenXrPlatformHelperLinux() = default;
OpenXrPlatformHelperLinux::~OpenXrPlatformHelperLinux() = default;

bool OpenXrPlatformHelperLinux::Initialize() {
  // No platform-specific initialization is needed on Linux; the base class
  // creates extension_enumeration_ after this returns true.
  return true;
}

std::unique_ptr<OpenXrGraphicsBinding>
OpenXrPlatformHelperLinux::GetGraphicsBinding() {
  return std::make_unique<OpenXrGraphicsBindingVulkan>(
      GetExtensionEnumeration());
}

void OpenXrPlatformHelperLinux::GetPlatformCreateInfo(
    const device::OpenXrCreateInfo& create_info,
    PlatformCreateInfoReadyCallback result_callback,
    PlatormInitiatedShutdownCallback /*shutdown_callback*/) {
  // Linux has no platform-specific XrInstanceCreateInfo.next struct.
  std::move(result_callback).Run(nullptr);
}

device::mojom::XRDeviceData OpenXrPlatformHelperLinux::GetXRDeviceData() {
  device::mojom::XRDeviceData data;
  // Ask the runtime rather than hardcoding; a Linux runtime may drive an
  // AR-capable headset. Answer false when there is no instance to query
  // instead of creating one here just to answer.
  data.is_ar_blend_mode_supported =
      xr_instance_ != XR_NULL_HANDLE && IsArBlendModeSupported(xr_instance_);
  return data;
}

void OpenXrPlatformHelperLinux::PrepareForSessionShutdown(
    base::OnceClosure shutdown_ready_callback) {
  std::move(shutdown_ready_callback).Run();
}

XrResult OpenXrPlatformHelperLinux::CreateInstance(XrInstance* instance,
                                                   void* create_info) {
  // Linux has no platform-specific XrInstanceCreateInfo::next (see
  // GetPlatformCreateInfo), and the enabled extension list does not depend on
  // it, so a session instance would be created exactly like the polling one.
  CHECK(!create_info);
  if (xr_instance_ != XR_NULL_HANDLE) {
    *instance = xr_instance_;
    return XR_SUCCESS;
  }
  return OpenXrPlatformHelper::CreateInstance(instance, create_info);
}

bool OpenXrPlatformHelperLinux::EnsurePollingInstance() {
  // Reuse the cached instance if one already exists.
  if (xr_instance_ != XR_NULL_HANDLE) {
    return true;
  }
  // Create an instance and keep it alive for subsequent availability polls
  // rather than recreating one every 5 seconds. A session adopts this same
  // instance (see CreateInstance) and destroys it when it ends.
  XrInstance instance = XR_NULL_HANDLE;
  return XR_SUCCEEDED(OpenXrPlatformHelper::CreateInstance(&instance, nullptr));
}

bool OpenXrPlatformHelperLinux::IsApiAvailable() {
  return EnsurePollingInstance();
}

bool OpenXrPlatformHelperLinux::IsHardwareAvailable() {
  // After a session ends, xr_instance_ is destroyed; re-create a cached
  // instance for polling.
  if (!EnsurePollingInstance()) {
    return false;
  }
  XrSystemId system;
  return XR_SUCCEEDED(OpenXrApiWrapper::GetSystem(xr_instance_, &system));
}

}  // namespace device
