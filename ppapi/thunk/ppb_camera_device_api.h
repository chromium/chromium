// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_CAMERA_DEVICE_API_H_
#define PPAPI_THUNK_PPB_CAMERA_DEVICE_API_H_

#include <stdint.h>

#include "ppapi/c/private/ppb_camera_device_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_CameraDevice_API {
 public:
  virtual ~PPB_CameraDevice_API() {}
  virtual int32_t Open(PP_Var device_id,
                       const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual void Close() = 0;
  virtual int32_t GetCameraCapabilities(
      PP_Resource* capabilities,
      const scoped_refptr<TrackedCallback>& callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_CAMERA_DEVICE_API_H_
