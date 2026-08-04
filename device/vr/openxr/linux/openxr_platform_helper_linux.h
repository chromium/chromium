// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_LINUX_OPENXR_PLATFORM_HELPER_LINUX_H_
#define DEVICE_VR_OPENXR_LINUX_OPENXR_PLATFORM_HELPER_LINUX_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "device/vr/openxr/openxr_platform_helper.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/vr_export.h"

namespace device {

// Linux platform helper for OpenXR: runs in the isolated XR utility process
// and talks to an externally-managed runtime via the Khronos OpenXR loader.
class DEVICE_VR_EXPORT OpenXrPlatformHelperLinux : public OpenXrPlatformHelper {
 public:
  OpenXrPlatformHelperLinux();

  OpenXrPlatformHelperLinux(const OpenXrPlatformHelperLinux&) = delete;
  OpenXrPlatformHelperLinux& operator=(const OpenXrPlatformHelperLinux&) =
      delete;

  ~OpenXrPlatformHelperLinux() override;

  // OpenXrPlatformHelper (pure virtuals):
  std::unique_ptr<OpenXrGraphicsBinding> GetGraphicsBinding() override;
  void GetPlatformCreateInfo(
      const device::OpenXrCreateInfo& create_info,
      PlatformCreateInfoReadyCallback result_callback,
      PlatormInitiatedShutdownCallback shutdown_callback) override;
  device::mojom::XRDeviceData GetXRDeviceData() override;
  void PrepareForSessionShutdown(
      base::OnceClosure shutdown_ready_callback) override;

  // Parallel to OpenXrPlatformHelperWindows — not virtual on the base.
  // Called from xr_runtime_provider.cc's Linux-specific block.
  bool IsApiAvailable();
  bool IsHardwareAvailable();

  // Override to destroy any cached polling instance before creating a new
  // session instance (base class CHECKs xr_instance_ == NULL).
  using OpenXrPlatformHelper::CreateInstance;  // unhide 1-arg overload
  XrResult CreateInstance(XrInstance* instance, void* create_info) override;

 protected:
  // OpenXrPlatformHelper:
  bool Initialize() override;

 private:
  // Ensures a cached XrInstance exists for availability polling, reusing the
  // live one if there is any (the base class stores it in xr_instance_).
  // Returns true if xr_instance_ is usable afterward.
  bool EnsurePollingInstance();
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_LINUX_OPENXR_PLATFORM_HELPER_LINUX_H_
