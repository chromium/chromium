// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_statics.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

OpenXrStatics::OpenXrStatics() : instance_(XR_NULL_HANDLE) {}

OpenXrStatics::~OpenXrStatics() {
  if (instance_ != XR_NULL_HANDLE) {
    xrDestroyInstance(instance_);
    instance_ = XR_NULL_HANDLE;
  }
}

XrInstance OpenXrStatics::GetXrInstance() {
  if (instance_ == XR_NULL_HANDLE &&
      XR_FAILED(CreateInstance(&instance_, extension_enumeration_))) {
    return XR_NULL_HANDLE;
  }
  return instance_;
}

bool OpenXrStatics::IsHardwareAvailable() {
  if (GetXrInstance() == XR_NULL_HANDLE) {
    return false;
  }

  XrSystemId system;
  return XR_SUCCEEDED(GetSystem(instance_, &system));
}

bool OpenXrStatics::IsApiAvailable() {
  return GetXrInstance() != XR_NULL_HANDLE;
}

#if defined(OS_WIN)
// Returns the LUID of the adapter the OpenXR runtime is on. Returns {0, 0} if
// the LUID could not be determined.
LUID OpenXrStatics::GetLuid(const OpenXrExtensionHelper& extension_helper) {
  if (GetXrInstance() == XR_NULL_HANDLE)
    return {0, 0};

  XrSystemId system;
  if (XR_FAILED(GetSystem(instance_, &system)))
    return {0, 0};

  if (extension_helper.ExtensionMethods().xrGetD3D11GraphicsRequirementsKHR ==
      nullptr)
    return {0, 0};

  XrGraphicsRequirementsD3D11KHR graphics_requirements = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
  if (XR_FAILED(
          extension_helper.ExtensionMethods().xrGetD3D11GraphicsRequirementsKHR(
              instance_, system, &graphics_requirements)))
    return {0, 0};

  return graphics_requirements.adapterLuid;
}
#endif

}  // namespace device
