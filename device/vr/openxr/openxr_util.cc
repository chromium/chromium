// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_util.h"

#include <d3d11.h>
#include <string>

#include "third_party/openxr/src/include/openxr/openxr_platform.h"

namespace device {

XrPosef PoseIdentity() {
  XrPosef pose{};
  pose.orientation.w = 1;
  return pose;
}

XrResult GetSystem(XrInstance instance, XrSystemId* system) {
  XrSystemGetInfo system_info = {XR_TYPE_SYSTEM_GET_INFO};
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  return xrGetSystem(instance, &system_info, system);
}

XrResult CreateInstance(XrInstance* instance) {
  XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};
  strcpy_s(instance_create_info.applicationInfo.applicationName, "Chromium");
  instance_create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

  // xrCreateInstance validates the list of extensions and returns
  // XR_ERROR_EXTENSION_NOT_PRESENT if an extension is not supported,
  // so we don't need to call xrEnumerateInstanceExtensionProperties
  // to validate these extensions.
  const char* extensions[] = {
      XR_KHR_D3D11_ENABLE_EXTENSION_NAME,
  };

  instance_create_info.enabledExtensionCount =
      sizeof(extensions) / sizeof(extensions[0]);
  instance_create_info.enabledExtensionNames = extensions;

  return xrCreateInstance(&instance_create_info, instance);
}

}  // namespace device
