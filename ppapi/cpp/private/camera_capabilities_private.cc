// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/cpp/private/camera_capabilities_private.h"

#include <stdint.h>

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <>
const char* interface_name<PPB_CameraCapabilities_Private_0_1>() {
  return PPB_CAMERACAPABILITIES_PRIVATE_INTERFACE_0_1;
}

}  // namespace

CameraCapabilities_Private::CameraCapabilities_Private() {
}

CameraCapabilities_Private::CameraCapabilities_Private(
    const CameraCapabilities_Private& other)
    : Resource(other) {
}

CameraCapabilities_Private::CameraCapabilities_Private(const Resource& resource)
    : Resource(resource) {
  PP_DCHECK(IsCameraCapabilities(resource));
}

CameraCapabilities_Private::CameraCapabilities_Private(PassRef,
                                                       PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

CameraCapabilities_Private::~CameraCapabilities_Private() {
}

void CameraCapabilities_Private::GetSupportedVideoCaptureFormats(
    std::vector<PP_VideoCaptureFormat>* formats) {
  if (!has_interface<PPB_CameraCapabilities_Private_0_1>()) {
    PP_DCHECK(false);
    return;
  }

  uint32_t array_size;
  PP_VideoCaptureFormat* array;
  get_interface<PPB_CameraCapabilities_Private_0_1>()
      ->GetSupportedVideoCaptureFormats(pp_resource(), &array_size, &array);
  formats->clear();
  formats->reserve(array_size);
  for (uint32_t i = 0; i < array_size; i++) {
    formats->push_back(array[i]);
  }
}

// static
bool CameraCapabilities_Private::IsCameraCapabilities(
    const Resource& resource) {
  if (!has_interface<PPB_CameraCapabilities_Private_0_1>())
    return false;

  return PP_ToBool(
      get_interface<PPB_CameraCapabilities_Private_0_1>()->IsCameraCapabilities(
          resource.pp_resource()));
}

}  // namespace pp
