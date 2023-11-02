// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_device_ref_dev.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_device_ref_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsDeviceRef(PP_Resource resource) {
  VLOG(4) << "PPB_DeviceRef_Dev::IsDeviceRef()";
  EnterResource<PPB_DeviceRef_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_DeviceType_Dev GetType(PP_Resource device_ref) {
  VLOG(4) << "PPB_DeviceRef_Dev::GetType()";
  EnterResource<PPB_DeviceRef_API> enter(device_ref, true);
  if (enter.failed())
    return PP_DEVICETYPE_DEV_INVALID;
  return enter.object()->GetType();
}

struct PP_Var GetName(PP_Resource device_ref) {
  VLOG(4) << "PPB_DeviceRef_Dev::GetName()";
  EnterResource<PPB_DeviceRef_API> enter(device_ref, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetName();
}

const PPB_DeviceRef_Dev_0_1 g_ppb_deviceref_dev_thunk_0_1 = {
    &IsDeviceRef, &GetType, &GetName};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_DeviceRef_Dev_0_1*
GetPPB_DeviceRef_Dev_0_1_Thunk() {
  return &g_ppb_deviceref_dev_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
