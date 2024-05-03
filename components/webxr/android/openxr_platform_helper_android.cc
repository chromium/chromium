// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/webxr/android/openxr_platform_helper_android.h"

#include <vector>

#include "base/android/jni_android.h"
#include "components/webxr/android/webxr_utils.h"
#include "components/webxr/android/xr_session_coordinator.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/openxr/android/openxr_graphics_binding_open_gles.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"

namespace webxr {

namespace {
static PFN_xrInitializeLoaderKHR g_initialize_loader_fn_ = nullptr;
}  // anonymous namespace

OpenXrPlatformHelperAndroid::OpenXrPlatformHelperAndroid()
    : session_coordinator_(std::make_unique<XrSessionCoordinator>()) {}

OpenXrPlatformHelperAndroid::~OpenXrPlatformHelperAndroid() = default;

std::unique_ptr<device::OpenXrGraphicsBinding>
OpenXrPlatformHelperAndroid::GetGraphicsBinding() {
  return std::make_unique<device::OpenXrGraphicsBindingOpenGLES>();
}

void OpenXrPlatformHelperAndroid::GetPlatformCreateInfo(
    const device::OpenXrCreateInfo& create_info,
    PlatformCreateInfoReadyCallback result_callback,
    PlatormInitiatedShutdownCallback shutdown_callback) {
  auto activity_ready_callback =
      base::BindOnce(&OpenXrPlatformHelperAndroid::OnXrActivityReady,
                     base::Unretained(this), std::move(result_callback));
  session_coordinator_->RequestXrSession(std::move(activity_ready_callback),
                                         std::move(shutdown_callback));
}

void OpenXrPlatformHelperAndroid::OnXrActivityReady(
    PlatformCreateInfoReadyCallback callback,
    const base::android::JavaParamRef<jobject>& activity) {
  activity_ = activity;

  create_info_.next = nullptr;
  create_info_.applicationVM = base::android::GetVM();
  create_info_.applicationActivity = activity_.obj();

  std::move(callback).Run(&create_info_);
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

bool OpenXrPlatformHelperAndroid::CheckHardwareSupport(
    content::WebContents* web_contents) {
  if (!device::features::IsOpenXrArEnabled()) {
    return true;
  }

  XrInstance instance = XR_NULL_HANDLE;
  if (!XR_SUCCEEDED(CreateTemporaryInstance(&instance, web_contents))) {
    return false;
  }

  is_ar_blend_mode_supported_ = IsArBlendModeSupported(instance);

  // Ensure that we destroy the temporary instance we created.
  return XR_SUCCEEDED(DestroyInstance(instance));
}

device::mojom::XRDeviceData OpenXrPlatformHelperAndroid::GetXRDeviceData() {
  device::mojom::XRDeviceData device_data;
  device_data.is_ar_blend_mode_supported = is_ar_blend_mode_supported_;
  return device_data;
}

XrResult OpenXrPlatformHelperAndroid::CreateTemporaryInstance(
    XrInstance* instance,
    content::WebContents* web_contents) {
  if (!web_contents) {
    return XR_ERROR_VALIDATION_FAILURE;
  }

  activity_ =
      XrSessionCoordinator::GetActivity(web_contents->GetJavaWebContents());

  XrInstanceCreateInfoAndroidKHR create_info{
      XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  create_info.next = nullptr;
  create_info.applicationVM = base::android::GetVM();
  create_info.applicationActivity = activity_.obj();

  return CreateInstance(instance, &create_info);
}

void OpenXrPlatformHelperAndroid::OnInstanceCreateFailure() {
  // Note that this may be called in the normal case of failing to create a
  // "temporary" instance that we were using solely to check support, and so
  // StartXrSession may not have been called yet; however, this method just
  // forwards the call to the corresponding Java class who appropriately no-ops
  // if there is no active session.
  session_coordinator_->EndSession();
}

XrResult OpenXrPlatformHelperAndroid::DestroyInstance(XrInstance& instance) {
  session_coordinator_->EndSession();

  // The `EndSession` call above can cause us to get called re-entrantly. The
  // base class `DestroyInstance` takes `instance` by reference and expects that
  // it is not null. In the case of a re-entrant call occurring, that call could
  // go through and null out the instance before our call to the base
  // `DestroyInstance` here, so verify that the `instance` is still valid before
  // attempting to destroy it.
  XrResult result = XR_SUCCESS;
  if (instance != XR_NULL_HANDLE) {
    result = OpenXrPlatformHelper::DestroyInstance(instance);
  }

  // Since we can't validate that we were only ever called with a valid
  // instance, we want to assert that the cached member is cleared as the result
  // of at least *one* successful call to the base `DestroyInstance`.
  if (XR_SUCCEEDED(result)) {
    CHECK(xr_instance_ == XR_NULL_HANDLE);
  }
  activity_ = nullptr;
  return result;
}

}  // namespace webxr
