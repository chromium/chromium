// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/webxr/android/openxr_platform_helper_android.h"

#include <vector>

#include "base/android/jni_android.h"
#include "components/webxr/android/webxr_utils.h"
#include "components/webxr/android/xr_session_coordinator.h"
#include "device/vr/openxr/android/openxr_graphics_binding_open_gles.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"

namespace webxr {

namespace {
static PFN_xrInitializeLoaderKHR g_initialize_loader_fn_ = nullptr;
}  // anonymous namespace

OpenXrPlatformHelperAndroid::OpenXrPlatformHelperAndroid() = default;
OpenXrPlatformHelperAndroid::~OpenXrPlatformHelperAndroid() = default;

std::unique_ptr<device::OpenXrGraphicsBinding>
OpenXrPlatformHelperAndroid::GetGraphicsBinding() {
  return std::make_unique<device::OpenXrGraphicsBindingOpenGLES>();
}

const void* OpenXrPlatformHelperAndroid::GetPlatformCreateInfo(
    const device::OpenXrCreateInfo& create_info) {
  // Re-compute the create_info_ that we need every time in case the activity
  // has changed.
  activity_ = XrSessionCoordinator::GetActivity(GetJavaWebContents(
      create_info.render_process_id, create_info.render_frame_id));

  create_info_.next = nullptr;
  create_info_.applicationVM = base::android::GetVM();
  create_info_.applicationActivity = activity_.obj();
  return &create_info_;
}

bool OpenXrPlatformHelperAndroid::Initialize() {
  XrResult result = XR_SUCCESS;
  if (g_initialize_loader_fn_ == nullptr) {
    result =
        xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                              (PFN_xrVoidFunction*)(&g_initialize_loader_fn_));
    if (XR_FAILED(result)) {
      LOG(ERROR) << __func__ << " Could not get loader initialization method";
      return false;
    }
  }

  app_context_ = XrSessionCoordinator::GetApplicationContext();
  XrLoaderInitInfoAndroidKHR loader_init_info;
  loader_init_info.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
  loader_init_info.next = nullptr;
  loader_init_info.applicationVM = base::android::GetVM();
  loader_init_info.applicationContext = app_context_.obj();
  result = g_initialize_loader_fn_(
      (const XrLoaderInitInfoBaseHeaderKHR*)&loader_init_info);
  if (XR_FAILED(result)) {
    LOG(ERROR) << "Initialize Loader failed with: " << result;
    return false;
  }

  return true;
}

device::mojom::XRDeviceData OpenXrPlatformHelperAndroid::GetXRDeviceData() {
  device::mojom::XRDeviceData device_data;
  device_data.is_ar_blend_mode_supported = false;
  return device_data;
}

}  // namespace webxr
