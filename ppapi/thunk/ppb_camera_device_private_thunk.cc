// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_camera_device_private.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_camera_device_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_camera_device_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_CameraDevice_Private::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateCameraDevicePrivate(instance);
}

PP_Bool IsCameraDevice(PP_Resource resource) {
  VLOG(4) << "PPB_CameraDevice_Private::IsCameraDevice()";
  EnterResource<PPB_CameraDevice_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Open(PP_Resource camera_device,
             struct PP_Var device_id,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_CameraDevice_Private::Open()";
  EnterResource<PPB_CameraDevice_API> enter(camera_device, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Open(device_id, enter.callback()));
}

void Close(PP_Resource camera_device) {
  VLOG(4) << "PPB_CameraDevice_Private::Close()";
  EnterResource<PPB_CameraDevice_API> enter(camera_device, true);
  if (enter.failed())
    return;
  enter.object()->Close();
}

int32_t GetCameraCapabilities(PP_Resource camera_device,
                              PP_Resource* capabilities,
                              struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_CameraDevice_Private::GetCameraCapabilities()";
  EnterResource<PPB_CameraDevice_API> enter(camera_device, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetCameraCapabilities(capabilities, enter.callback()));
}

const PPB_CameraDevice_Private_0_1 g_ppb_cameradevice_private_thunk_0_1 = {
    &Create, &IsCameraDevice, &Open, &Close, &GetCameraCapabilities};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_CameraDevice_Private_0_1*
GetPPB_CameraDevice_Private_0_1_Thunk() {
  return &g_ppb_cameradevice_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
