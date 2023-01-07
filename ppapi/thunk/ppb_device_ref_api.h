// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_DEVICE_REF_API_H_
#define PPAPI_THUNK_PPB_DEVICE_REF_API_H_

#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct DeviceRefData;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_DeviceRef_API {
 public:
  virtual ~PPB_DeviceRef_API() {}

  // This function is not exposed through the C API, but returns the internal
  // data for easy proxying.
  virtual const DeviceRefData& GetDeviceRefData() const = 0;

  virtual PP_DeviceType_Dev GetType() = 0;
  virtual PP_Var GetName() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_DEVICE_REF_API_H_
