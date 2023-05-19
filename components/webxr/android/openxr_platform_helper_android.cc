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
static bool g_has_loader_been_initialized_ = false;
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
  if (g_has_loader_been_initialized_) {
    return true;
  }

  PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
  XrResult result =
      xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                            (PFN_xrVoidFunction*)(&initializeLoader));
  if (XR_FAILED(result)) {
    LOG(ERROR) << __func__ << " Could not get loader initialization method";
    return false;
  }

  app_context_ = XrSessionCoordinator::GetApplicationContext();
  XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
  loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
  loaderInitInfoAndroid.next = nullptr;
  loaderInitInfoAndroid.applicationVM = base::android::GetVM();
  loaderInitInfoAndroid.applicationContext = app_context_.obj();
  result = initializeLoader(
      (const XrLoaderInitInfoBaseHeaderKHR*)&loaderInitInfoAndroid);
  if (XR_FAILED(result)) {
    LOG(ERROR) << "Initialize Loader failed with: " << result;
    return false;
  }

  g_has_loader_been_initialized_ = true;
  return true;
}

device::mojom::XRDeviceData OpenXrPlatformHelperAndroid::GetXRDeviceData() {
  device::mojom::XRDeviceData device_data;
  device_data.is_ar_blend_mode_supported = false;
  return device_data;
}

}  // namespace webxr
