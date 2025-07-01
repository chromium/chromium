// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/camera_device_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/private/camera_capabilities_private.h"

namespace pp {

namespace {

template <>
const char* interface_name<PPB_CameraDevice_Private_0_1>() {
  return PPB_CAMERADEVICE_PRIVATE_INTERFACE_0_1;
}

}  // namespace

CameraDevice_Private::CameraDevice_Private() {
}

CameraDevice_Private::CameraDevice_Private(const CameraDevice_Private& other)
    : Resource(other) {
}

CameraDevice_Private::CameraDevice_Private(const Resource& resource)
    : Resource(resource) {
  PP_DCHECK(IsCameraDevice(resource));
}

CameraDevice_Private::CameraDevice_Private(const InstanceHandle& instance) {
  if (has_interface<PPB_CameraDevice_Private_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_CameraDevice_Private_0_1>()->Create(
            instance.pp_instance()));
    return;
  }
  PP_DCHECK(false);
}

CameraDevice_Private::CameraDevice_Private(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

CameraDevice_Private::~CameraDevice_Private() {
}

int32_t CameraDevice_Private::Open(const Var& device_id,
                                   const CompletionCallback& callback) {
  if (!has_interface<PPB_CameraDevice_Private_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  return get_interface<PPB_CameraDevice_Private_0_1>()->Open(
      pp_resource(), device_id.pp_var(), callback.pp_completion_callback());
}

void CameraDevice_Private::Close() {
  if (has_interface<PPB_CameraDevice_Private_0_1>())
    get_interface<PPB_CameraDevice_Private_0_1>()->Close(pp_resource());
}

int32_t CameraDevice_Private::GetCameraCapabilities(
    const CompletionCallbackWithOutput<CameraCapabilities_Private>& callback) {
  if (!has_interface<PPB_CameraDevice_Private_0_1>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  return get_interface<PPB_CameraDevice_Private_0_1>()->GetCameraCapabilities(
      pp_resource(), callback.output(), callback.pp_completion_callback());
}

// static
bool CameraDevice_Private::IsCameraDevice(const Resource& resource) {
  if (!has_interface<PPB_CameraDevice_Private_0_1>())
    return false;

  return PP_ToBool(
      get_interface<PPB_CameraDevice_Private_0_1>()->IsCameraDevice(
          resource.pp_resource()));
}

}  // namespace pp
