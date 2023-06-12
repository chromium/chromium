// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_VIDEO_CAPTURE_API_H_
#define PPAPI_THUNK_PPB_VIDEO_CAPTURE_API_H_

#include <stdint.h>

#include <string>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_VideoCapture_API {
 public:
  virtual ~PPB_VideoCapture_API() {}

  virtual int32_t EnumerateDevices(const PP_ArrayOutput& output,
                                   scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t MonitorDeviceChange(PP_MonitorDeviceChangeCallback callback,
                                      void* user_data) = 0;
  virtual int32_t Open(const std::string& device_id,
                       const PP_VideoCaptureDeviceInfo_Dev& requested_info,
                       uint32_t buffer_count,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t StartCapture() = 0;
  virtual int32_t ReuseBuffer(uint32_t buffer) = 0;
  virtual int32_t StopCapture() = 0;
  virtual void Close() = 0;

  // This function is not exposed through the C API.  It is only used by flash
  // to make synchronous device enumeration.
  virtual int32_t EnumerateDevicesSync(const PP_ArrayOutput& devices) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_VIDEO_CAPTURE_API_H_
