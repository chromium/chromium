// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/windows/openxr_platform_helper_windows.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/windows/openxr_graphics_binding_d3d11.h"
#include "device/vr/openxr/windows/openxr_instance_wrapper.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace {

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
}  // anonymous namespace

// static
void OpenXrPlatformHelper::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
  // If we are in an app container, we must require the app container extension
  // to ensure robust execution of the OpenXR runtime
  if (IsRunningInWin32AppContainer()) {
    // Add the win32 app container compatible extension to our list of
    // extensions. If this runtime does not support execution in an app
    // container environment, one of xrCreateInstance or
    // xrOpenXrApiWrapper::GetSystem will fail.
    extensions.push_back(XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME);
  }
}

// static
std::vector<const char*> OpenXrPlatformHelper::GetOptionalExtensions() {
  std::vector<const char*> extensions;
  extensions.push_back(
      XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME);
  return extensions;
}

OpenXrPlatformHelperWindows::OpenXrPlatformHelperWindows() = default;
OpenXrPlatformHelperWindows::~OpenXrPlatformHelperWindows() = default;

std::unique_ptr<OpenXrGraphicsBinding>
OpenXrPlatformHelperWindows::GetGraphicsBinding() {
  return std::make_unique<OpenXrGraphicsBindingD3D11>(
      weak_ptr_factory_.GetWeakPtr());
}

void OpenXrPlatformHelperWindows::GetPlatformCreateInfo(
    const device::OpenXrCreateInfo& create_info,
    PlatformCreateInfoReadyCallback result_callback,
    PlatormInitiatedShutdownCallback shutdown_callback) {
  // We have nothing we need to add to the "next" chain.
  std::move(result_callback).Run(nullptr);
}

device::mojom::XRDeviceData OpenXrPlatformHelperWindows::GetXRDeviceData() {
  device::mojom::XRDeviceData device_data;
  device_data.is_ar_blend_mode_supported =
      IsArBlendModeSupported(GetOrCreateXrInstance());
  // Only set the LUID if it exists and is nonzero.
  if (LUID luid; TryGetLuid(&luid)) {
    device_data.luid = CHROME_LUID{luid.LowPart, luid.HighPart};
  }

  return device_data;
}

// Returns the LUID of the adapter the OpenXR runtime is on. Returns false and
// sets luid to {0, 0} if the LUID could not be determined. Also returns false
// if the value of the retrieved LUID is {0, 0}.
bool OpenXrPlatformHelperWindows::TryGetLuid(LUID* luid, XrSystemId system) {
  CHECK(luid);
  XrInstance instance = GetOrCreateXrInstance();
  if (instance == XR_NULL_HANDLE) {
    return false;
  }

  if (system == XR_NULL_SYSTEM_ID &&
      XR_FAILED(OpenXrApiWrapper::GetSystem(instance, &system))) {
    return false;
  }

  if (get_graphics_requirements_fn_ == nullptr) {
    if (XR_FAILED(xrGetInstanceProcAddr(
            instance, "xrGetD3D11GraphicsRequirementsKHR",
            (PFN_xrVoidFunction*)(&get_graphics_requirements_fn_))) ||
        get_graphics_requirements_fn_ == nullptr) {
      return false;
    }
  }

  XrGraphicsRequirementsD3D11KHR graphics_requirements = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
  if (XR_FAILED(get_graphics_requirements_fn_(instance, system,
                                              &graphics_requirements))) {
    return false;
  }

  *luid = graphics_requirements.adapterLuid;
  return luid->HighPart != 0 || luid->LowPart != 0;
}

bool OpenXrPlatformHelperWindows::IsHardwareAvailable() {
  XrInstance instance = GetOrCreateXrInstance();
  if (instance == XR_NULL_HANDLE) {
    return false;
  }

  XrSystemId system;
  return XR_SUCCEEDED(OpenXrApiWrapper::GetSystem(instance, &system));
}

bool OpenXrPlatformHelperWindows::IsApiAvailable() {
  return GetOrCreateXrInstance() != XR_NULL_HANDLE;
}

bool OpenXrPlatformHelperWindows::Initialize() {
  // Nothing to do;
  return true;
}

XrResult OpenXrPlatformHelperWindows::CreateInstance(XrInstance* instance,
                                                     void* create_info) {
  CHECK(instance);

  // The base-class expects CreatInstance to be called exactly once without a
  // matching DestroyInstance call. However, since we don't actually need the
  // `create_info`, and we *do* need the instance for a few other checks that
  // happen before the Device would normally request a session, we may have
  // already generated an instance, so just return that if it already exists.
  // Note that due to some issues with some runtimes we can't *actually* destroy
  // the instance, so we rely on a singleton rather than xr_instance_ member.
  // Full details are in `OpenXrInstanceWrapper`'s class description.
  auto* instance_wrapper = OpenXrInstanceWrapper::GetWrapper();
  if (instance_wrapper->HasXrInstance()) {
    *instance = instance_wrapper->GetXrInstance();
    return XR_SUCCESS;
  }

  // Since one hasn't already been created, we can use the logic from the parent
  // class to actually create the instance, we just need to store it before we
  // return it.
  XrResult result = OpenXrPlatformHelper::CreateInstance(instance, create_info);
  if (XR_SUCCEEDED(result)) {
    // xr_instance_ should have been set by CreateInstance.
    instance_wrapper->SetXrInstance(*instance);
  }

  return result;
}

XrInstance OpenXrPlatformHelperWindows::GetOrCreateXrInstance() {
  XrInstance instance = XR_NULL_HANDLE;
  // CreateInstance will return the cached instance if there is one. If
  // CreateInstance fails, we'll just end up returning XR_NULL_HANDLE which is
  // fine. We don't actually need anything from the OpenXrCreateInfo to create
  // an instance on Windows.
  (void)CreateInstance(&instance, nullptr);
  return instance;
}

XrResult OpenXrPlatformHelperWindows::DestroyInstance(XrInstance& instance) {
  CHECK(instance != XR_NULL_HANDLE);
  // Since the XrInstance is a singleton, we don't actually destroy it here.
  // For more context, see the class description of `OpenXrInstanceWrapper`.
  instance = XR_NULL_HANDLE;
  xr_instance_ = XR_NULL_HANDLE;
  return XR_SUCCESS;
}

}  // namespace device
