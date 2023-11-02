// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_CAMERA_CAPABILITIES_API_H_
#define PPAPI_THUNK_PPB_CAMERA_CAPABILITIES_API_H_

#include <stdint.h>

#include "ppapi/c/private/ppb_camera_capabilities_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_CameraCapabilities_API {
 public:
  virtual ~PPB_CameraCapabilities_API() {}
  virtual void GetSupportedVideoCaptureFormats(
      uint32_t* array_size,
      PP_VideoCaptureFormat** formats) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_CAMERA_CAPABILITIES_API_H_
