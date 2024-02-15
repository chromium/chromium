// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_WINDOWS_OPENXR_PLATFORM_HELPER_WINDOWS_H_
#define DEVICE_VR_OPENXR_WINDOWS_OPENXR_PLATFORM_HELPER_WINDOWS_H_

#include "device/vr/openxr/openxr_platform_helper.h"

#include "base/memory/weak_ptr.h"
#include "base/win/windows_types.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrGraphicsBinding;

// Windows-specific implementation of the OpenXrPlatformHelper.
class DEVICE_VR_EXPORT OpenXrPlatformHelperWindows
    : public OpenXrPlatformHelper {
 public:
  OpenXrPlatformHelperWindows();
  ~OpenXrPlatformHelperWindows() override;

  // OpenXrPlatformHelper
  std::unique_ptr<OpenXrGraphicsBinding> GetGraphicsBinding() override;
  void GetPlatformCreateInfo(
      const device::OpenXrCreateInfo& create_info,
      PlatformCreateInfoReadyCallback result_callback,
      PlatormInitiatedShutdownCallback shutdown_callback) override;
  device::mojom::XRDeviceData GetXRDeviceData() override;
  bool Initialize() override;

  // Note that we treat the XrInstance as a singleton on Windows, so we must
  // override CreateInstance/DestroyInstance. See `OpenXrInstanceWrapper` for
  // more context.
  XrResult CreateInstance(XrInstance* instance, void* create_info) override;
  XrResult DestroyInstance(XrInstance& instance) override;

  // Methods used by the XrRuntimeProvider to determine if an OpenXr session
  // could be created/supported.
  bool IsHardwareAvailable();
  bool IsApiAvailable();

  // Called by the D3D11 GraphicsBinding to set up the texture helper and also
  // used when creating the XRDeviceData.
  bool TryGetLuid(LUID* luid, XrSystemId system = XR_NULL_SYSTEM_ID);

 private:
  XrInstance GetOrCreateXrInstance();

  // Accessing the LUID is handled via an extension method. We cache that
  // method here once we've loaded it so that we don't have to look it up again,
  // since we expect at least two calls to this method.
  PFN_xrGetD3D11GraphicsRequirementsKHR get_graphics_requirements_fn_ = nullptr;

  // Must be last.
  base::WeakPtrFactory<OpenXrPlatformHelperWindows> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_WINDOWS_OPENXR_PLATFORM_HELPER_WINDOWS_H_
