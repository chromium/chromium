// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_device_ref_shared.h"

#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"

using ppapi::thunk::PPB_DeviceRef_API;

namespace ppapi {

DeviceRefData::DeviceRefData() : type(PP_DEVICETYPE_DEV_INVALID) {}

PPB_DeviceRef_Shared::PPB_DeviceRef_Shared(ResourceObjectType type,
                                           PP_Instance instance,
                                           const DeviceRefData& data)
    : Resource(type, instance), data_(data) {}

PPB_DeviceRef_API* PPB_DeviceRef_Shared::AsPPB_DeviceRef_API() { return this; }

const DeviceRefData& PPB_DeviceRef_Shared::GetDeviceRefData() const {
  return data_;
}

PP_DeviceType_Dev PPB_DeviceRef_Shared::GetType() { return data_.type; }

PP_Var PPB_DeviceRef_Shared::GetName() {
  return StringVar::StringToPPVar(data_.name);
}

}  // namespace ppapi
