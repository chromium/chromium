// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_DEVICE_REF_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_DEVICE_REF_SHARED_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_device_ref_api.h"

namespace ppapi {

// IF YOU ADD STUFF TO THIS CLASS
// ==============================
// Be sure to add it to the STRUCT_TRAITS at the top of ppapi_messages.h.
struct PPAPI_SHARED_EXPORT DeviceRefData {
  DeviceRefData();

  bool operator==(const DeviceRefData& other) const {
    return type == other.type && name == other.name && id == other.id;
  }

  PP_DeviceType_Dev type;
  std::string name;
  std::string id;
};

class PPAPI_SHARED_EXPORT PPB_DeviceRef_Shared
    : public Resource,
      public thunk::PPB_DeviceRef_API {
 public:
  PPB_DeviceRef_Shared() = delete;

  PPB_DeviceRef_Shared(ResourceObjectType type,
                       PP_Instance instance,
                       const DeviceRefData& data);

  PPB_DeviceRef_Shared(const PPB_DeviceRef_Shared&) = delete;
  PPB_DeviceRef_Shared& operator=(const PPB_DeviceRef_Shared&) = delete;

  // Resource overrides.
  PPB_DeviceRef_API* AsPPB_DeviceRef_API() override;

  // PPB_DeviceRef_API implementation.
  const DeviceRefData& GetDeviceRefData() const override;
  PP_DeviceType_Dev GetType() override;
  PP_Var GetName() override;

 private:
  DeviceRefData data_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_DEVICE_REF_SHARED_H_
